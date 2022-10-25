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

#include "act-iter.h" // monster_near_iterator
#include "areas.h"
#include "art-enum.h"
#include "coordit.h"
#include "directn.h"
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
#include "skills.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-other.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "spl-zap.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "rltiles/tiledef-gui.h"    // spell tiles
#include "tag-version.h"
#include "tiles-build-specific.h"
#include "transform.h"

struct spell_desc
{
    spell_type id;
    const char  *title;
    spschools_type disciplines;
    spell_flags flags;       // bitfield
    unsigned int level;

    // Usually in the range 0..200 (0 means uncapped).
    // Note that some spells are also capped through zap_type.
    // See spell_power_cap below.
    int power_cap;

    // At power 0, you get min_range. At power power_cap, you get max_range.
    int min_range;
    int max_range;

    // Some spells have a noise at their place of effect, in addition
    // to their casting noise.
    // For zap-based spells, effect_noise is used automatically (if it exists)
    // on hit. For all other spells, it needs to be called manually.
    int effect_noise;

    /// Icon for the spell in e.g. spellbooks, casting menus, etc.
    tileidx_t tile;
};

// TODO: apply https://isocpp.org/wiki/faq/ctors#construct-on-first-use-v2
// to spelldata
#include "spl-data.h"

// spell_list is a static vector mapping spell_type enum values into positions
// in spelldata. It is not initialized until game startup, so can't be relied
// on in static constructors. (E.g., all spells register as invalid until
// `init_spell_descs` has been run.)
static vector<int> &_get_spell_list()
{
    // https://isocpp.org/wiki/faq/ctors#construct-on-first-use-v2
    static vector<int> spell_list(NUM_SPELLS, -1);
    return spell_list;
}

#define SPELLDATASIZE ARRAYSZ(spelldata)

static const struct spell_desc *_seekspell(spell_type spellid);

//
//             BEGIN PUBLIC FUNCTIONS
//

// All this does is merely refresh the internal spell list {dlb}:
void init_spell_descs()
{
    vector<int> &spell_list = _get_spell_list();

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

        ASSERTM(!(data.flags & spflag::targeting_mask)
                || (data.min_range >= 0 && data.max_range > 0),
                "targeted/directed spell '%s' has invalid range", data.title);

        if (!spell_removed(data.id)
            && data.id != SPELL_NO_SPELL
            && data.id != SPELL_DEBUGGING_RAY)
        {
            ASSERTM(!(data.flags & spflag::monster && is_player_spell(data.id)),
                "spell '%s' is declared as a monster spell but is a player spell", data.title);

            ASSERTM(!(!(data.flags & spflag::monster) && !is_player_spell(data.id)),
                "spell '%s' is not declared as a monster spell but is not a player spell", data.title);
        }

        spell_list[data.id] = i;
    }
}

typedef map<string, spell_type> spell_name_map;

static spell_name_map &_get_spell_name_cache()
{
    static spell_name_map spell_name_cache;
    return spell_name_cache;
}

void init_spell_name_cache()
{
    spell_name_map &cache = _get_spell_name_cache();
    for (int i = 0; i < NUM_SPELLS; i++)
    {
        spell_type type = static_cast<spell_type>(i);

        if (!is_valid_spell(type))
            continue;

        const char *sptitle = spell_title(type);
        ASSERT(sptitle);
        const string spell_name = lowercase_string(sptitle);
        cache[spell_name] = type;
    }
}

bool spell_data_initialized()
{
    return _get_spell_name_cache().size() > 0;
}

spell_type spell_by_name(string name, bool partial_match)
{
    if (name.empty())
        return SPELL_NO_SPELL;

    lowercase(name);

    if (!partial_match)
        return lookup(_get_spell_name_cache(), name, SPELL_NO_SPELL);

    const spell_type sp = find_earliest_match(name, SPELL_NO_SPELL, NUM_SPELLS,
                                              is_valid_spell, spell_title);
    return sp == NUM_SPELLS ? SPELL_NO_SPELL : sp;
}

spschool school_by_name(string name)
{
    spschool short_match, long_match;
    int                short_matches, long_matches;

    short_match   = long_match   = spschool::none;
    short_matches = long_matches = 0;

    lowercase(name);

    for (int i = 0; i <= (int)spschool::random; i++)
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
        return spschool::none;

    if (short_matches == 1 && long_matches != 1)
        return short_match;
    if (short_matches != 1 && long_matches == 1)
        return long_match;

    if (short_match == long_match)
        return short_match;

    return spschool::none;
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
    if (vehumet_is_offering(spell))
        library_add_spells({ spell });

    int slot_i;
    int letter_j = -1;
    string sname = spell_title(spell);
    lowercase(sname);
    // first we find a slot in our head:
    for (slot_i = 0; slot_i < MAX_KNOWN_SPELLS; slot_i++)
    {
        if (you.spells[slot_i] == SPELL_NO_SPELL)
            break;
    }

    you.spells[slot_i] = spell;

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
                const int new_letter = letter_to_index(ch);
                const int existing_slot = you.spell_letter_table[new_letter];
                if (existing_slot == -1)
                {
                    letter_j = new_letter;
                    break;
                }
                else if (overwrite)
                {
                    const string ename = lowercase_string(
                            spell_title(get_spell_by_letter(ch)));
                    // Don't overwrite a spell matched by the same rule.
                    if (!entry.first.matches(ename))
                    {
                        letter_j = new_letter;
                        break;
                    }
                }
                // Otherwise continue on to the next letter in this rule.
            }
        }
        if (letter_j != -1)
            break;
    }
    // If we didn't find a label above, choose the first available one.
    if (letter_j == -1)
        for (letter_j = 0; letter_j < 52; letter_j++)
        {
            if (you.spell_letter_table[letter_j] == -1)
                break;
        }

    if (you.num_turns)
        mprf("Spell assigned to '%c'.", index_to_letter(letter_j));

    // Swapping with an existing spell.
    if (you.spell_letter_table[letter_j] != -1)
    {
        // Find a spot for the spell being moved. Assumes there will be one.
        for (int free_letter = 0; free_letter < 52; free_letter++)
            if (you.spell_letter_table[free_letter] == -1)
            {
                you.spell_letter_table[free_letter] = you.spell_letter_table[letter_j];
                break;
            }
    }

    you.spell_letter_table[letter_j] = slot_i;

    you.spell_no++;

    take_note(Note(NOTE_LEARN_SPELL, spell));

    spell_skills(spell, you.skills_to_show);

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
    update_screen();
#endif

    return true;
}

bool del_spell_from_memory_by_slot(int slot)
{
    ASSERT_RANGE(slot, 0, MAX_KNOWN_SPELLS);

    if (you.last_cast_spell == you.spells[slot])
        you.last_cast_spell = SPELL_NO_SPELL;

    spell_skills(you.spells[slot], you.skills_to_hide);

    mprf("Your memory of %s unravels.", spell_title(you.spells[slot]));

    you.spells[slot] = SPELL_NO_SPELL;

    for (int j = 0; j < 52; j++)
        if (you.spell_letter_table[j] == slot)
            you.spell_letter_table[j] = -1;

    you.spell_no--;

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
    update_screen();
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

// Checks if the spell is an explosion that can be placed anywhere even without
// an unobstructed beam path, such as fire storm.
bool spell_is_direct_explosion(spell_type spell)
{
    return spell == SPELL_FIRE_STORM || spell == SPELL_CALL_DOWN_DAMNATION
           || spell == SPELL_GHOSTLY_SACRIFICE || spell == SPELL_UPHEAVAL
           || spell == SPELL_ERUPTION;
}

bool spell_harms_target(spell_type spell)
{
    const spell_flags flags = _seekspell(spell)->flags;

    if (flags & (spflag::helpful | spflag::neutral))
        return false;

    if (flags & spflag::targeting_mask)
        return true;

    // n.b. this excludes various untargeted attack spells like hailstorm, abs 0

    return false;
}

bool spell_harms_area(spell_type spell)
{
    const spell_flags flags = _seekspell(spell)->flags;

    if (flags & (spflag::helpful | spflag::neutral))
        return false;

    if (flags & spflag::area)
        return true;

    return false;
}

/**
 * Does the spell cause damage directly on a successful, non-resisted, cast?
 * This is much narrower than "harm", and excludes e.g. hexes that harm in a
 * broader sense.
 */
bool spell_is_direct_attack(spell_type spell)
{
    if (spell_harms_target(spell))
    {
        // spell school exceptions
        if (spell == SPELL_VIOLENT_UNRAVELLING  // hex
            || spell == SPELL_FORCE_LANCE // transloc
            || spell == SPELL_GRAVITAS
            || spell == SPELL_BLINKBOLT
            || spell == SPELL_BANISHMENT)
        {
            return true;
        }

        // spell schools that generally "harm" but not damage
        if (get_spell_disciplines(spell) & (spschool::hexes | spschool::translocation | spschool::summoning))
            return false;

        // harms target exceptions outside of those schools
        if (spell == SPELL_PETRIFY
            || spell == SPELL_STICKY_FLAME // maybe ok?
            || spell == SPELL_FREEZING_CLOUD // some targeted cloud spells that do indirect damage via the clouds
            || spell == SPELL_MEPHITIC_CLOUD
            || spell == SPELL_POISONOUS_CLOUD)
        {
            return false;
        }

        return true;
    }

    // The area harm check has too many false positives to bother with here
    if (spell == SPELL_ISKENDERUNS_MYSTIC_BLAST
        || spell == SPELL_OZOCUBUS_REFRIGERATION
        || spell == SPELL_SYMBOL_OF_TORMENT
        || spell == SPELL_SHATTER
        || spell == SPELL_DISCHARGE
        || spell == SPELL_ARCJOLT
        || spell == SPELL_CHAIN_LIGHTNING
        || spell == SPELL_DRAIN_LIFE
        || spell == SPELL_CHAIN_OF_CHAOS
        || spell == SPELL_IRRADIATE
        || spell == SPELL_IGNITION
        || spell == SPELL_STARBURST
        || spell == SPELL_HAILSTORM
        || spell == SPELL_MANIFOLD_ASSAULT
        || spell == SPELL_MAXWELLS_COUPLING) // n.b. not an area spell
    {
        return true;
    }
    return false;
}

// How much MP does it cost for the player to cast this spell?
//
// @param real_spell  True iff the player is casting the spell normally,
// not via an evocable or other odd source.
int spell_mana(spell_type which_spell, bool real_spell)
{
    const int level = _seekspell(which_spell)->level;
    if (real_spell && (you.duration[DUR_BRILLIANCE]
                       || player_equip_unrand(UNRAND_FOLLY)))
    {
        return level/2 + level%2; // round up
    }
    return level;
}

// applied in naughties (more difficult = higher level knowledge = worse)
// and triggers for Sif acting (same reasoning as above, just good) {dlb}
int spell_difficulty(spell_type which_spell)
{
    return _seekspell(which_spell)->level;
}

int spell_levels_required(spell_type which_spell)
{
    return spell_difficulty(which_spell);
}

spell_flags get_spell_flags(spell_type which_spell)
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

bool spell_typematch(spell_type which_spell, spschool which_disc)
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

/*
 * Place clouds over an area with a function.
 *
 * @param func        A function called to place each cloud.
 * @param where       The starting location of the cloud. A targeter_cloud
 *                    with aim set to this location is used to determine the
 *                    affected locations.
 * @param pow         The spellpower of the spell placing the clouds, which
 *                    determines how long the cloud will last.
 * @param number      How many clouds to place in total. Only this number will
 *                    be placed regardless
 * @param ctype       The type of cloud to place.
 * @param agent       Any agent that may have caused the cloud. If this is the
 *                    player, god conducts are applied.
 * @param spread_rate How quickly the cloud spreads.
 * @param excl_rad    How large of an exclusion radius to make around the
 *                    cloud.
*/
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
        // Check for user cancel in interactive direction choosing.
        if (spelld.isCancel && spelld.interactive && !spelld.fire_context)
            canned_msg(MSG_OK);
        return false;
    }

    pbolt.set_target(spelld);
    pbolt.source = you.pos();

    return true;
}

const char* spelltype_short_name(spschool which_spelltype)
{
    switch (which_spelltype)
    {
    case spschool::conjuration:
        return "Conj";
    case spschool::hexes:
        return "Hex";
    case spschool::fire:
        return "Fire";
    case spschool::ice:
        return "Ice";
    case spschool::transmutation:
        return "Tmut";
    case spschool::necromancy:
        return "Necr";
    case spschool::summoning:
        return "Summ";
    case spschool::translocation:
        return "Tloc";
    case spschool::poison:
        return "Pois";
    case spschool::earth:
        return "Erth";
    case spschool::air:
        return "Air";
    case spschool::random:
        return "Rndm";
    default:
        return "Bug";
    }
}

const char* spelltype_long_name(spschool which_spelltype)
{
    switch (which_spelltype)
    {
    case spschool::conjuration:
        return "Conjuration";
    case spschool::hexes:
        return "Hexes";
    case spschool::fire:
        return "Fire";
    case spschool::ice:
        return "Ice";
    case spschool::transmutation:
        return "Transmutation";
    case spschool::necromancy:
        return "Necromancy";
    case spschool::summoning:
        return "Summoning";
    case spschool::translocation:
        return "Translocation";
    case spschool::poison:
        return "Poison";
    case spschool::earth:
        return "Earth";
    case spschool::air:
        return "Air";
    case spschool::random:
        return "Random";
    default:
        return "Bug";
    }
}

skill_type spell_type2skill(spschool spelltype)
{
    switch (spelltype)
    {
    case spschool::conjuration:    return SK_CONJURATIONS;
    case spschool::hexes:          return SK_HEXES;
    case spschool::fire:           return SK_FIRE_MAGIC;
    case spschool::ice:            return SK_ICE_MAGIC;
    case spschool::transmutation:  return SK_TRANSMUTATIONS;
    case spschool::necromancy:     return SK_NECROMANCY;
    case spschool::summoning:      return SK_SUMMONINGS;
    case spschool::translocation:  return SK_TRANSLOCATIONS;
    case spschool::poison:         return SK_POISON_MAGIC;
    case spschool::earth:          return SK_EARTH_MAGIC;
    case spschool::air:            return SK_AIR_MAGIC;

    default:
        dprf("spell_type2skill: called with unmapped spell school %u"
             " (name '%s')", static_cast<unsigned int>(spelltype),
             spelltype_long_name(spelltype));
        return SK_NONE;
    }
}

spschool skill2spell_type(skill_type spell_skill)
{
    switch (spell_skill)
    {
    case SK_CONJURATIONS:    return spschool::conjuration;
    case SK_HEXES:           return spschool::hexes;
    case SK_FIRE_MAGIC:      return spschool::fire;
    case SK_ICE_MAGIC:       return spschool::ice;
    case SK_TRANSMUTATIONS:  return spschool::transmutation;
    case SK_NECROMANCY:      return spschool::necromancy;
    case SK_SUMMONINGS:      return spschool::summoning;
    case SK_TRANSLOCATIONS:  return spschool::translocation;
    case SK_POISON_MAGIC:    return spschool::poison;
    case SK_EARTH_MAGIC:     return spschool::earth;
    case SK_AIR_MAGIC:       return spschool::air;

    default:
        return spschool::none;
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
    const int index = _get_spell_list()[spell];
    ASSERT(index != -1);

    return &spelldata[index];
}

bool is_valid_spell(spell_type spell)
{
    return spell > SPELL_NO_SPELL && spell < NUM_SPELLS
           && _get_spell_list()[spell] != -1;
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

int spell_range(spell_type spell, int pow,
                bool allow_bonus, bool ignore_shadows)
{
    int minrange = _seekspell(spell)->min_range;
    int maxrange = _seekspell(spell)->max_range;

    const int range_cap = ignore_shadows ? you.normal_vision
                                         : you.current_vision;

    ASSERT(maxrange >= minrange);

    // spells with no range have maxrange == minrange == -1
    if (maxrange < 0)
        return maxrange;

    if (allow_bonus
        && vehumet_supports_spell(spell)
        && have_passive(passive_t::spells_range)
        && maxrange > 1
        && spell != SPELL_HAILSTORM // uses a special system
        && spell != SPELL_THUNDERBOLT) // lightning rod only
    {
        maxrange++;
        minrange++;
    }

    if (minrange == maxrange)
        return min(minrange, range_cap);

    const int powercap = spell_power_cap(spell);

    if (powercap <= pow)
        return min(maxrange, range_cap);

    // Round appropriately.
    return min(range_cap,
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
    const spell_flags flags = get_spell_flags(spell);

    if (testbits(flags, spflag::silent))
        return 0;

    return spell_difficulty(spell);
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
    const int noise = _seekspell(spell)->effect_noise;

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

    return noise;
}

/**
 * Casting-specific checks that are involved when casting any spell. Includes
 * MP (which does use the spell level if provided), confusion state, banned
 * schools.
 *
 * @param spell      The spell in question.
 * @param temp       Include checks for volatile or temporary states
 *                   (status effects, mana)
 * @return           Whether casting is useless
 */
bool casting_is_useless(spell_type spell, bool temp)
{
    return !casting_uselessness_reason(spell, temp).empty();
}

/**
 * Casting-specific checks that are involved when casting any spell or larger
 * groups of spells (e.g. entire schools). Includes MP (which does use the
 * spell level if provided), confusion state, banned schools.
 *
 * @param spell      The spell in question.
 * @param temp       Include checks for volatile or temporary states
 *                   (status effects, mana)
 # @return           A reason why casting is useless, or "" if it isn't.
 */
string casting_uselessness_reason(spell_type spell, bool temp)
{
    if (temp)
    {
        if (you.duration[DUR_CONF] > 0)
            return "you're too confused to cast spells.";

        if (spell_difficulty(spell) > you.experience_level)
            return "you aren't experienced enough to cast this spell.";

        if (you.has_mutation(MUT_HP_CASTING))
        {
            // TODO: deduplicate with enough_hp()
            if (you.duration[DUR_DEATHS_DOOR])
                return "you cannot pay life while functionally dead.";
            if (!enough_hp(spell_mana(spell), true, false))
                return "you don't have enough health to cast this spell.";
        }
        else if (!enough_mp(spell_mana(spell), true, false))
            return "you don't have enough magic to cast this spell.";

        if (spell == SPELL_SUBLIMATION_OF_BLOOD
            && you.magic_points == you.max_magic_points)
        {
            if (you.has_mutation(MUT_HP_CASTING))
                return "your magic and health are inextricable.";
            return "your reserves of magic are already full.";
        }
    }

    // Check for banned schools (Currently just Ru sacrifices)
    if (cannot_use_schools(get_spell_disciplines(spell)))
        return "you cannot use spells of this school.";

    // TODO: these checks were in separate places, but is this already covered
    // by cannot_use_schools?
    if (get_spell_disciplines(spell) & spschool::summoning
        && you.allies_forbidden())
    {
        return "you cannot coerce anything to answer your summons.";
    }

    // other ally spells not affected by the school checks
    switch (spell)
    {
    case SPELL_ANIMATE_DEAD:
    case SPELL_DEATH_CHANNEL:
    case SPELL_SIMULACRUM:
    case SPELL_INFESTATION:
    case SPELL_TUKIMAS_DANCE:
        if (you.allies_forbidden())
            return "you cannot coerce anything to obey you.";
        break;
    default:
        break;
    }

    return "";
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
 * @param skip_casting_checks true if the spell is evoked or from an innate or
 *                   divine ability, or if casting checks are carried out
 *                   already.
 *                   false if it is a spell being cast normally.
 * @return           Whether the given spell has no chance of being useful.
 */
bool spell_is_useless(spell_type spell, bool temp, bool prevent,
                      bool skip_casting_checks)
{
    return !spell_uselessness_reason(spell, temp, prevent, skip_casting_checks).empty();
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
 * @param skip_casting_checks true if the spell is evoked or from an innate or
 *                   divine ability, or if casting checks are carried out
 *                   already.
 *                   false if it is a spell being cast normally.
 * @return           The reason a spell is useless to the player, if it is;
 *                   "" otherwise. The string should be a full clause, but
 *                   begin with a lowercase letter so callers can put it in
 *                   the middle of a sentence.
 */
string spell_uselessness_reason(spell_type spell, bool temp, bool prevent,
                                bool skip_casting_checks)
{
    // prevent all of this logic during excursions / levelgen. This function
    // does get called during character creation, so allow it to run for !temp.
    if (temp && (!in_bounds(you.pos()) || !you.on_current_level))
        return "you can't cast spells right now.";

    if (!skip_casting_checks)
    {
        string c_check = casting_uselessness_reason(spell, temp);
        if (!c_check.empty())
            return c_check;
    }

    if (!prevent && temp && spell_no_hostile_in_range(spell))
        return "you can't see any hostile targets that would be affected.";

    switch (spell)
    {
    case SPELL_BLINK:
        // XXX: this is a little redundant with you_no_tele_reason()
        // but trying to sort out temp and so on is a mess
        if (you.stasis())
            return "your stasis prevents you from teleporting.";

        // Distinct from no_tele - can still be forcibly blinked.
        if (temp && you.duration[DUR_BLINK_COOLDOWN])
            return "you are still too unstable to blink.";

        if (temp && you.no_tele(true))
            return lowercase_first(you.no_tele_reason(true));
        break;

    case SPELL_SWIFTNESS:
        if (you.stasis())
            return "your stasis precludes magical swiftness.";

        if (temp)
        {
            if (you.duration[DUR_SWIFTNESS])
                return "this spell is already in effect.";
            if (player_movement_speed(false) <= FASTEST_PLAYER_MOVE_SPEED)
                return "you're already travelling as fast as you can.";
            if (!you.is_motile())
                return "you can't move.";
        }
        break;

    case SPELL_STATUE_FORM:
        if (SP_GARGOYLE == you.species)
            return "you're already a statue.";
        // fallthrough to other forms

    case SPELL_BEASTLY_APPENDAGE:
    case SPELL_BLADE_HANDS:
    case SPELL_DRAGON_FORM:
    case SPELL_ICE_FORM:
    case SPELL_STORM_FORM:
    case SPELL_SPIDER_FORM:
        if (you.undead_state(temp) == US_UNDEAD)
            return "your undead flesh cannot be transformed.";
        if (you.is_lifeless_undead(temp))
            return "your current blood level is not sufficient.";
        break;

    case SPELL_PORTAL_PROJECTILE:
        if (you.has_mutation(MUT_NO_GRASPING))
            return "this spell is useless without hands.";
        break;
    case SPELL_LEDAS_LIQUEFACTION:
        if (temp && you.duration[DUR_LIQUEFYING])
            return "you need to wait for the ground to become solid again.";
        break;

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
        // only prohibited to actual undead, not lichformed players
        if (you.undead_state(false))
            return "you're too dead.";
        break;

    case SPELL_OZOCUBUS_ARMOUR:
        if (temp && you.form == transformation::statue)
            return "the film of ice won't work on stone.";
        if (temp && player_equip_unrand(UNRAND_SALAMANDER))
            return "your ring of flames would instantly melt the ice.";
        break;

    case SPELL_SUBLIMATION_OF_BLOOD:
        if (!you.can_bleed(temp))
            return "you have no blood to sublime.";
        break;

    case SPELL_POLAR_VORTEX:
        if (temp && (you.duration[DUR_VORTEX]
                     || you.duration[DUR_VORTEX_COOLDOWN]))
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
        if (temp && cast_malign_gateway(&you, 0, GOD_NO_GOD, false, true)
                    == spret::abort)
        {
            return "you need more open space to create a gateway.";
        }
        break;

    case SPELL_SUMMON_FOREST:
        if (temp && you.duration[DUR_FORESTED])
            return "you can only summon one forest at a time.";
        if (temp && cast_summon_forest(&you, 0, GOD_NO_GOD, false, true) == spret::abort)
            return "you need more open space to fit a forest.";
        break;

    case SPELL_PASSWALL:
        // the full check would need a real spellpower here, so we just check
        // a drastically simplified version of it
        if (!temp)
            break;
        if (!you.is_motile())
            return "you can't move.";
        if (!passwall_simplified_check(you))
            return "you aren't next to any passable walls.";
        if (you.is_constricted())
            return "you're being held away from the wall.";
        break;

    case SPELL_ANIMATE_DEAD:
    case SPELL_SIMULACRUM:
        if (have_passive(passive_t::goldify_corpses))
            return "necromancy does not work on golden corpses.";
        if (have_passive(passive_t::reaping))
            return "you are already reaping souls!";
        break;

    case SPELL_DEATH_CHANNEL:
        if (temp && you.duration[DUR_DEATH_CHANNEL])
            return "you are already channeling the dead.";
        if (have_passive(passive_t::reaping))
            return "you are already reaping souls!";
        break;

    case SPELL_ROT:
        {
            const mon_holy_type holiness = you.holiness(temp);
            if (holiness != MH_NATURAL && holiness != MH_UNDEAD)
                return "you have no flesh to rot.";
        }
        // fallthrough to cloud spells
    case SPELL_BLASTSPARK:
    case SPELL_POISONOUS_CLOUD:
    case SPELL_FREEZING_CLOUD:
    case SPELL_MEPHITIC_CLOUD:
        if (temp && env.level_state & LSTATE_STILL_WINDS)
            return "the air is too still for clouds to form.";
        break;

    case SPELL_GOLUBRIAS_PASSAGE:
        if (temp && player_in_branch(BRANCH_GAUNTLET))
        {
            return "a magic seal in the Gauntlet prevents this spell "
                   "from working.";
        }
        break;

    case SPELL_DRAGON_CALL:
        if (temp && (you.duration[DUR_DRAGON_CALL]
                     || you.duration[DUR_DRAGON_CALL_COOLDOWN]))
        {
            return "you cannot issue another dragon's call so soon.";
        }
        break;

    case SPELL_FROZEN_RAMPARTS:
        if (temp && you.duration[DUR_FROZEN_RAMPARTS])
            return "you cannot sustain more frozen ramparts right now.";
        break;

    case SPELL_WEREBLOOD:
        if (you.undead_state(temp) == US_UNDEAD
            || you.is_lifeless_undead(temp))
        {
            return "you lack blood to transform.";
        }
        break;

    case SPELL_NOXIOUS_BOG:
        if (temp && you.duration[DUR_NOXIOUS_BOG])
            return "you cannot sustain more bogs right now.";
        break;

    case SPELL_ANIMATE_ARMOUR:
        if (you_can_wear(EQ_BODY_ARMOUR, temp) == MB_FALSE)
            return "you cannot wear body armour.";
        if (temp && !you.slot_item(EQ_BODY_ARMOUR))
            return "you have no body armour to summon the spirit of.";
        break;

    case SPELL_MANIFOLD_ASSAULT:
        if (temp)
        {
            const string unproj_reason = weapon_unprojectability_reason();
            if (unproj_reason != "")
                return unproj_reason;
        }
        break;

    case SPELL_MOMENTUM_STRIKE:
        if (temp && !you.is_motile())
            return "you cannot redirect your momentum while unable to move.";
        break;

    case SPELL_ELECTRIC_CHARGE:
        if (temp)
        {
            const string no_move_reason = movement_impossible_reason();
            if (!no_move_reason.empty())
                return no_move_reason;
            if (!electric_charge_possible(true))
                return "you can't see anything to charge at.";
        }
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
    // sanity check: various things below will be prone to crash in these cases.
    if (!in_bounds(you.pos()) || !you.on_current_level)
        return true;

    const int range = calc_spell_range(spell, 0);
    const int minRange = get_dist_to_nearest_monster();
    const int pow = calc_spell_power(spell, true, false, true);

    switch (spell)
    {
    // These don't target monsters or can target features.
    case SPELL_APPORTATION:
    case SPELL_PASSWALL:
    case SPELL_GOLUBRIAS_PASSAGE:
    // case SPELL_LRD: // TODO: LRD logic here is a bit confusing, it should error
    //                 // now that it doesn't destroy walls
    case SPELL_FULMINANT_PRISM:
    case SPELL_SUMMON_LIGHTNING_SPIRE:
    case SPELL_NOXIOUS_BOG:
    // This can always potentially hit out-of-LOS, although this is conditional
    // on spell-power.
    case SPELL_FIRE_STORM:
        return false;

    case SPELL_OLGREBS_TOXIC_RADIANCE:
    case SPELL_IGNITION:
    case SPELL_FROZEN_RAMPARTS:
        return minRange > you.current_vision;

    case SPELL_POISONOUS_VAPOURS:
    {
        // can this just be turned into a zap at this point?
        dist test_targ;
        for (radius_iterator ri(you.pos(), range, C_SQUARE, LOS_NO_TRANS);
             ri; ++ri)
        {
            test_targ.target = *ri;
            const monster* mons = monster_at(*ri);
            if (mons && !mons->wont_attack()
                && cast_poisonous_vapours(0, test_targ, true, true)
                                                            == spret::success)
            {
                return false;
            }
        }
        return true;
    }

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
        return cast_ignite_poison(&you, -1, false, true) == spret::abort;

    case SPELL_STARBURST:
        return cast_starburst(-1, false, true) == spret::abort;

    case SPELL_HAILSTORM:
        return cast_hailstorm(-1, false, true) == spret::abort;

    case SPELL_DAZZLING_FLASH:
        return cast_dazzling_flash(pow, false, true) == spret::abort;

     case SPELL_MAXWELLS_COUPLING:
         return cast_maxwells_coupling(pow, false, true) == spret::abort;

     case SPELL_INTOXICATE:
         return cast_intoxicate(-1, false, true) == spret::abort;

    case SPELL_MANIFOLD_ASSAULT:
         return cast_manifold_assault(-1, false, false) == spret::abort;

    case SPELL_OZOCUBUS_REFRIGERATION:
         return trace_los_attack_spell(SPELL_OZOCUBUS_REFRIGERATION, pow, &you)
             == spret::abort;

    case SPELL_ARCJOLT:
        for (coord_def t : arcjolt_targets(you, pow, false))
        {
            const monster *mon = monster_at(t);
            if (mon != nullptr && !mon->wont_attack())
                return false;
        }
        return true;

    case SPELL_CHAIN_LIGHTNING:
        for (coord_def t : chain_lightning_targets())
        {
            const monster *mon = monster_at(t);
            if (mon != nullptr && !mon->wont_attack())
                return false;
        }
        return true;

    case SPELL_SCORCH:
        return find_near_hostiles(range, false).empty();

    case SPELL_ANGUISH:
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        {
            const monster &mon = **mi;
            if (you.can_see(mon)
                && mons_intel(mon) > I_BRAINLESS
                && mon.willpower() != WILL_INVULN
                && !mons_atts_aligned(you.temp_attitude(), mon.attitude)
                && !mon.has_ench(ENCH_ANGUISH))
            {
                return false;
            }

        }
        return true; // TODO

    default:
        break;
    }

    if (minRange < 0 || range < 0)
        return false;

    const spell_flags flags = get_spell_flags(spell);

    // The healing spells.
    if (testbits(flags, spflag::helpful))
        return false;

    // For chosing default targets and prompting we don't treat Inner Flame as
    // neutral, since the seeping flames trigger conducts and harm the monster
    // before it explodes.
    const bool allow_friends = testbits(flags, spflag::neutral)
                               || spell == SPELL_INNER_FLAME;

    bolt beam;
    beam.flavour = BEAM_VISUAL;
    beam.origin_spell = spell;

    zap_type zap = spell_to_zap(spell);
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

        const bool smite = testbits(flags, spflag::target);

        for (radius_iterator ri(you.pos(), range, C_SQUARE, LOS_DEFAULT);
             ri; ++ri)
        {
            tempbeam = beam;
            tempbeam.target = *ri;

            // For smite-targeted spells that aren't LOS-range.
            if (smite)
            {
                // XXX These are basic checks that might be applicable to
                // non-smiting spells as well. For those, the loop currently
                // relies mostly on results from the temp beam firing, but it
                // may be valid to exclude solid and non-reachable targets for
                // all spells. -gammafunk
                if (cell_is_solid(*ri) || !you.see_cell_no_trans(*ri))
                    continue;

                // XXX Currently Vile Clutch is the only smite-targeted area
                // spell that isn't LOS-range. Spell explosion radii are not
                // stored anywhere, defaulting to 1 for non-smite-targeting
                // spells through bolt::refine_for_explosions() or being set in
                // setup functions for the smite targeted explosions. It would
                // be good to move basic explosion radius info into spell_desc
                // or possibly zap_data. -gammafunk
                tempbeam.ex_size = tempbeam.is_explosion ? 1 : 0;
                tempbeam.explode();
            }
            else
                tempbeam.fire();

            if (tempbeam.foe_info.count > 0
                || allow_friends && tempbeam.friend_info.count > 0)
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
 * @param schools   A bitfield containing a union of spschools.
 * @return          Whether the player is unable use any of the given schools.
 */
bool cannot_use_schools(spschools_type schools)
{
    COMPILE_CHECK(ARRAYSZ(arcana_sacrifice_map) == SPSCHOOL_LAST_EXPONENT + 1);

    // iter over every school
    for (int i = 0; i <= SPSCHOOL_LAST_EXPONENT; i++)
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
    for (int exp = 0; exp <= SPSCHOOL_LAST_EXPONENT; exp++)
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

/* How to regenerate this:
   comm -2 -3 \
    <(clang -P -E -nostdinc -nobuiltininc spell-type.h -DTAG_MAJOR_VERSION=34 | sort) \
    <(clang -P -E -nostdinc -nobuiltininc spell-type.h -DTAG_MAJOR_VERSION=35 | sort) \
    | grep SPELL
*/
const set<spell_type> removed_spells =
{
#if TAG_MAJOR_VERSION == 34
    SPELL_AURA_OF_ABJURATION,
    SPELL_BOLT_OF_INACCURACY,
    SPELL_CHANT_FIRE_STORM,
    SPELL_CIGOTUVIS_DEGENERATION,
    SPELL_CIGOTUVIS_EMBRACE,
    SPELL_CONDENSATION_SHIELD,
    SPELL_CONTROLLED_BLINK,
    SPELL_CONTROL_TELEPORT,
    SPELL_CONTROL_UNDEAD,
    SPELL_CONTROL_WINDS,
    SPELL_CORRUPT_BODY,
    SPELL_CURE_POISON,
    SPELL_DEFLECT_MISSILES,
    SPELL_DELAYED_FIREBALL,
    SPELL_DEMONIC_HORDE,
    SPELL_DRACONIAN_BREATH,
    SPELL_EPHEMERAL_INFUSION,
    SPELL_EVAPORATE,
    SPELL_EXPLOSIVE_BOLT,
    SPELL_FAKE_RAKSHASA_SUMMON,
    SPELL_FIRE_BRAND,
    SPELL_FIRE_CLOUD,
    SPELL_FLY,
    SPELL_FORCEFUL_DISMISSAL,
    SPELL_FREEZING_AURA,
    SPELL_FRENZY,
    SPELL_FULSOME_DISTILLATION,
    SPELL_GRAND_AVATAR,
    SPELL_HASTE_PLANTS,
    SPELL_HOLY_LIGHT,
    SPELL_HOLY_WORD,
    SPELL_HOMUNCULUS,
    SPELL_HUNTING_CRY,
    SPELL_IGNITE_POISON_SINGLE,
    SPELL_INFUSION,
    SPELL_INSULATION,
    SPELL_IRON_ELEMENTALS,
    SPELL_LETHAL_INFUSION,
    SPELL_MELEE,
    SPELL_MIASMA_CLOUD,
    SPELL_MISLEAD,
    SPELL_PHASE_SHIFT,
    SPELL_POISON_CLOUD,
    SPELL_POISON_WEAPON,
    SPELL_RANDOM_BOLT,
    SPELL_REARRANGE_PIECES,
    SPELL_RECALL,
    SPELL_REGENERATION,
    SPELL_RESURRECT,
    SPELL_RING_OF_FLAMES,
    SPELL_SACRIFICE,
    SPELL_SCATTERSHOT,
    SPELL_SEE_INVISIBLE,
    SPELL_SERPENT_OF_HELL_BREATH_REMOVED,
    SPELL_SHAFT_SELF,
    SPELL_SHROUD_OF_GOLUBRIA,
    SPELL_SILVER_BLAST,
    SPELL_SINGULARITY,
    SPELL_SONG_OF_SHIELDING,
    SPELL_SPECTRAL_WEAPON,
    SPELL_STEAM_CLOUD,
    SPELL_STICKS_TO_SNAKES,
    SPELL_STONESKIN,
    SPELL_STRIKING,
    SPELL_SUMMON_BUTTERFLIES,
    SPELL_SUMMON_ELEMENTAL,
    SPELL_SUMMON_RAKSHASA,
    SPELL_SUMMON_SWARM,
    SPELL_SUMMON_TWISTER,
    SPELL_SUNRAY,
    SPELL_SURE_BLADE,
    SPELL_THROW,
    SPELL_TOMB_OF_DOROKLOHE,
    SPELL_VAMPIRE_SUMMON,
    SPELL_WARP_BRAND,
    SPELL_WEAVE_SHADOWS,
    SPELL_DARKNESS,
    SPELL_CLOUD_CONE,
    SPELL_RING_OF_THUNDER,
    SPELL_TWISTED_RESURRECTION,
    SPELL_RANDOM_EFFECTS,
    SPELL_HYDRA_FORM,
    SPELL_VORTEX,
    SPELL_GOAD_BEASTS,
    SPELL_TELEPORT_SELF,
    SPELL_EXCRUCIATING_WOUNDS,
    SPELL_CONJURE_FLAME,
    SPELL_CORPSE_ROT,
#endif
};

bool spell_removed(spell_type spell)
{
    return removed_spells.count(spell) != 0;
}

void end_wait_spells(bool quiet)
{
    end_searing_ray();
    end_maxwells_coupling(quiet);
    end_flame_wave();
}

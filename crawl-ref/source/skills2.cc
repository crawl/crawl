/**
 * @file
 * @brief More skill related functions.
**/

#include "AppHdr.h"

#include "skills2.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "artefact.h"
#include "describe.h"
#include "evoke.h"
#include "externs.h"
#include "godabil.h"
#include "libutil.h"
#include "player.h"
#include "religion.h"
#include "species.h"
#include "skills.h"
#include "skill_menu.h"
#include "unwind.h"

typedef string (*string_fn)();
typedef map<string, string_fn> skill_op_map;

static skill_op_map Skill_Op_Map;

// The species for which the skill title is being worked out.
static species_type Skill_Species = SP_UNKNOWN;

class skill_title_key_t
{
public:
    skill_title_key_t(const char *k, string_fn o) : key(k), op(o)
    {
        Skill_Op_Map[k] = o;
    }

    static string get(const string &_key)
    {
        skill_op_map::const_iterator i = Skill_Op_Map.find(_key);
        return (i == Skill_Op_Map.end()? string() : (i->second)());
    }
private:
    const char *key;
    string_fn op;
};

typedef skill_title_key_t stk;

// Basic goals for titles:
// The higher titles must come last.
// Referring to the skill itself is fine ("Transmuter") but not impressive.
// No overlaps, high diversity.

// Replace @Adj@ with uppercase adjective form, @genus@ with lowercase genus,
// @Genus@ with uppercase genus, and %s with special cases defined below,
// including but not limited to species.

// NOTE:  Even though %s could be used with most of these, remember that
// the character's race will be listed on the next line.  It's only really
// intended for cases where things might be really awkward without it. -- bwr

// NOTE: If a skill name is changed, remember to also adapt the database entry.
static const char *skills[NUM_SKILLS][6] =
{
  //  Skill name        levels 1-7       levels 8-14        levels 15-20       levels 21-26      level 27
    {"Fighting",       "Skirmisher",    "Fighter",         "Warrior",         "Slayer",         "Conqueror"},
    {"Short Blades",   "Cutter",        "Slicer",          "Swashbuckler",    "Cutthroat",      "Politician"},
    {"Long Blades",    "Slasher",       "Carver",          "Fencer",          "@Adj@ Blade",    "Swordmaster"},
    {"Axes",           "Chopper",       "Cleaver",         "Severer",         "Executioner",    "Axe Maniac"},
    {"Maces & Flails", "Cudgeler",      "Basher",          "Bludgeoner",      "Shatterer",      "Skullcrusher"},
    {"Polearms",       "Poker",         "Spear-Bearer",    "Impaler",         "Phalangite",     "@Adj@ Porcupine"},
    {"Staves",         "Twirler",       "Cruncher",        "Stickfighter",    "Pulveriser",     "Chief of Staff"},
    {"Slings",         "Vandal",        "Slinger",         "Whirler",         "Slingshot",      "@Adj@ Catapult"},
    {"Bows",           "Shooter",       "Archer",          "Marks@genus@",    "Crack Shot",     "Merry @Genus@"},
    {"Crossbows",      "Bolt Thrower",  "Quickloader",     "Sharpshooter",    "Sniper",         "@Adj@ Arbalest"},
    {"Throwing",       "Chucker",       "Thrower",         "Deadly Accurate", "Hawkeye",        "@Adj@ Ballista"},
    {"Armour",         "Covered",       "Protected",       "Tortoise",        "Impregnable",    "Invulnerable"},
    {"Dodging",        "Ducker",        "Nimble",          "Spry",            "Acrobat",        "Intangible"},
    {"Stealth",        "Sneak",         "Covert",          "Unseen",          "Imperceptible",  "Ninja"},
#if TAG_MAJOR_VERSION == 34
    {"Stabbing",       "Miscreant",     "Blackguard",      "Backstabber",     "Cutthroat",      "Politician"},
#endif
    {"Shields",        "Shield-Bearer", "Hoplite",         "Blocker",         "Peltast",        "@Adj@ Barricade"},
#if TAG_MAJOR_VERSION == 34
    {"Traps",          "Scout",         "Disarmer",        "Vigilant",        "Perceptive",     "Dungeon Master"},
#endif
    // STR based fighters, for DEX/martial arts titles see below.  Felids get their own category, too.
    {"Unarmed Combat", "Ruffian",       "Grappler",        "Brawler",         "Wrestler",       "@Weight@weight Champion"},

    {"Spellcasting",   "Magician",      "Thaumaturge",     "Eclecticist",     "Sorcerer",       "Archmage"},
    {"Conjurations",   "Ruinous",       "Conjurer",        "Destroyer",       "Devastator",     "Annihilator"},
    {"Hexes",          "Vexing",        "Jinx",            "Bewitcher",       "Maledictor",     "Spellbinder"},
    {"Charms",         "Charmwright",   "Infuser",         "Anointer",        "Gracecrafter",   "Miracle Worker"},
    {"Summonings",     "Caller",        "Summoner",        "Convoker",        "Demonologist",   "Hellbinder"},
    {"Necromancy",     "Grave Robber",  "Reanimator",      "Necromancer",     "Thanatomancer",  "@Genus_Short@ of Death"},
    {"Translocations", "Grasshopper",   "Placeless @Genus@", "Blinker",       "Portalist",      "Plane @Walker@"},
    {"Transmutations", "Changer",       "Transmogrifier",  "Alchemist",       "Malleable",      "Shapeless @Genus@"},

    {"Fire Magic",     "Firebug",       "Arsonist",        "Scorcher",        "Pyromancer",     "Infernalist"},
    {"Ice Magic",      "Chiller",       "Frost Mage",      "Gelid",           "Cryomancer",     "Englaciator"},
    {"Air Magic",      "Gusty",         "Cloud Mage",      "Aerator",         "Anemomancer",    "Meteorologist"},
    {"Earth Magic",    "Digger",        "Geomancer",       "Earth Mage",      "Metallomancer",  "Petrodigitator"},
    {"Poison Magic",   "Stinger",       "Tainter",         "Polluter",        "Contaminator",   "Envenomancer"},

    // These titles apply to atheists only, worshippers of the various gods
    // use the god titles instead, depending on piety or, in Xom's case, mood.
    {"Invocations",    "Unbeliever",    "Agnostic",        "Dissident",       "Heretic",        "Apostate"},
    {"Evocations",     "Charlatan",     "Prestidigitator", "Fetichist",       "Evocator",       "Talismancer"},
};

static const char *martial_arts_titles[6] =
    {"Unarmed Combat", "Insei", "Martial Artist", "Black Belt", "Sensei", "Grand Master"};
static const char *claw_and_tooth_titles[6] =
    {"Unarmed Combat", "Scratcher", "Gouger", "Ripper", "Eviscerator", "Sabretooth"};

struct species_skill_aptitude
{
    species_type species;
    skill_type   skill;
    int aptitude;          // -50..50, with 0 for humans

    species_skill_aptitude(species_type _species,
                           skill_type _skill,
                           int _aptitude)
        : species(_species), skill(_skill), aptitude(_aptitude)
    {
    }
};

#include "aptitudes.h"

// Traditionally, Spellcasting and In/Evocations formed the exceptions here:
// Spellcasting skill was more expensive with about 130%, the other two got
// a discount with about 75%.
static int _spec_skills[NUM_SPECIES][NUM_SKILLS];

int get_skill_progress(skill_type sk, int level, int points, int scale)
{
    if (level >= 27)
        return 0;

    const int needed = skill_exp_needed(level + 1, sk);
    const int prev_needed = skill_exp_needed(level, sk);
    if (needed == 0) // invalid race, legitimate at startup
        return 0;
    // A scale as small as 92 would overflow with 31 bits if skill_rdiv()
    // is involved: needed can be 91985, skill_rdiv() multiplies by 256.
    const int64_t amt_done = points - prev_needed;
    int prog = (amt_done * scale) / (needed - prev_needed);

    ASSERT(prog >= 0);

    return prog;
}

int get_skill_progress(skill_type sk, int scale)
{
    return get_skill_progress(sk, you.skills[sk], you.skill_points[sk], scale);
}

int get_skill_percentage(const skill_type x)
{
    return get_skill_progress(x, 100);
}

const char *skill_name(skill_type which_skill)
{
    return (skills[which_skill][0]);
}

skill_type str_to_skill(const string &skill)
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
        if (skills[i][0] && skill == skills[i][0])
            return (static_cast<skill_type>(i));

    return SK_FIGHTING;
}

static string _stk_adj_cap()
{
    return species_name(Skill_Species, false, true);
}

static string _stk_genus_cap()
{
    return species_name(Skill_Species, true, false);
}

static string _stk_genus_nocap()
{
    string s = _stk_genus_cap();
    return lowercase(s);
}

static string _stk_genus_short_cap()
{
    return (Skill_Species == SP_DEMIGOD ? "God" :
            _stk_genus_cap());
}

static string _stk_walker()
{
    return (Skill_Species == SP_NAGA     ? "Slider" :
            Skill_Species == SP_TENGU    ? "Glider" :
            Skill_Species == SP_DJINNI   ? "Floater" :
            Skill_Species == SP_OCTOPODE ? "Wriggler"
                                         : "Walker");
}

static string _stk_weight()
{
    switch (Skill_Species)
    {
    case SP_OGRE:
    case SP_TROLL:
        return "Heavy";

    case SP_NAGA:
    case SP_CENTAUR:
        return "Cruiser";

    default:
        return "Middle";

    case SP_HIGH_ELF:
    case SP_DEEP_ELF:
    case SP_SLUDGE_ELF:
    case SP_TENGU:
        return "Light";

    case SP_HALFLING:
    case SP_KOBOLD:
        return "Feather";

    case SP_SPRIGGAN:
        return "Fly";

    case SP_FELID:
        return "Bacteria"; // not used
    }
}

static skill_title_key_t _skill_title_keys[] =
{
    stk("Adj", _stk_adj_cap),
    stk("Genus", _stk_genus_cap),
    stk("genus", _stk_genus_nocap),
    stk("Genus_Short", _stk_genus_short_cap),
    stk("Walker", _stk_walker),
    stk("Weight", _stk_weight),
};

static string _replace_skill_keys(const string &text)
{
    // The container array is unused, we rely on side effects of constructors
    // of individual items.  Yay.
    UNUSED(_skill_title_keys);

    string::size_type at = 0, last = 0;
    ostringstream res;
    while ((at = text.find('@', last)) != string::npos)
    {
        res << text.substr(last, at - last);
        const string::size_type end = text.find('@', at + 1);
        if (end == string::npos)
            break;

        const string key = text.substr(at + 1, end - at - 1);
        const string value = stk::get(key);

        ASSERT(!value.empty());

        res << value;

        last = end + 1;
    }
    if (!last)
        return text;

    res << text.substr(last);
    return res.str();
}

unsigned get_skill_rank(unsigned skill_lev)
{
    // Translate skill level into skill ranking {dlb}:
    return ((skill_lev <= 7)  ? 0 :
                            (skill_lev <= 14) ? 1 :
                            (skill_lev <= 20) ? 2 :
                            (skill_lev <= 26) ? 3
                            /* level 27 */    : 4);
}

string skill_title_by_rank(skill_type best_skill, uint8_t skill_rank,
                           int species, int str, int dex, int god, int piety)
{
    // paranoia
    if (is_invalid_skill(best_skill))
        return "Adventurer";

    if (species == -1)
        species = you.species;

    if (str == -1)
        str = you.base_stats[STAT_STR];

    if (dex == -1)
        dex = you.base_stats[STAT_DEX];

    if (god == -1)
        god = you.religion;

    // Increment rank by one to "skip" skill name in array {dlb}:
    ++skill_rank;

    string result;

    if (best_skill < NUM_SKILLS)
    {
        // Note that ghosts default to (dex == str) and god == no_god, due
        // to a current lack of that information... the god case is probably
        // suitable for most cases (TSO/Zin/Ely at the very least). -- bwr
        switch (best_skill)
        {
        case SK_UNARMED_COMBAT:
            if (species == SP_FELID)
            {
                result = claw_and_tooth_titles[skill_rank];
                break;
            }
            result = (dex >= str) ? martial_arts_titles[skill_rank]
                                  : skills[best_skill][skill_rank];

            break;

        case SK_INVOCATIONS:
            if (god != GOD_NO_GOD)
                result = god_title((god_type)god, (species_type)species, piety);
            break;

        case SK_BOWS:
            if (player_genus(GENPC_ELVEN, static_cast<species_type>(species))
                && skill_rank == 5)
            {
                result = "Master Archer";
                break;
            }
            break;

        case SK_SPELLCASTING:
            if (species == SP_OGRE)
                result = "Ogre Mage";
            break;

        case SK_NECROMANCY:
            if (species == SP_SPRIGGAN && skill_rank == 5)
                result = "La Petite Mort";
            break;

        default:
            break;
        }
        if (result.empty())
            result = skills[best_skill][skill_rank];
    }

    {
        unwind_var<species_type> sp(Skill_Species,
                                    static_cast<species_type>(species));
        result = _replace_skill_keys(result);
    }

    return (result.empty() ? string("Invalid Title")
                           : result);
}

string skill_title(skill_type best_skill, uint8_t skill_lev,
                   int species, int str, int dex, int god, int piety)
{
    return skill_title_by_rank(best_skill, get_skill_rank(skill_lev),
                               species, str, dex, god, piety);
}

string player_title()
{
    const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    return skill_title(best, you.skills[ best ]);
}

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill)
{
    ASSERT(min_skill < NUM_SKILLS);
    ASSERT(max_skill < NUM_SKILLS);
    skill_type ret = SK_FIGHTING;
    unsigned int best_skill_level = 0;
    unsigned int best_position = 1000;

    for (int i = min_skill; i <= max_skill; i++)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (sk == excl_skill)
            continue;

        const unsigned int skill_level = you.skill(sk, 10, true);
        if (skill_level > best_skill_level)
        {
            ret = sk;
            best_skill_level = skill_level;
            best_position = you.skill_order[sk];

        }
        else if (skill_level == best_skill_level
                 && you.skill_order[sk] < best_position)
        {
            ret = sk;
            best_position = you.skill_order[sk];
        }
    }

    return ret;
}

// Calculate the skill_order array from scratch.
//
// The skill order array is used for breaking ties in best_skill.
// This is done by ranking each skill by the order in which it
// has attained its current level (the values are the number of
// skills at or above that level when the current skill reached it).
//
// In this way, the skill which has been at a level for the longest
// is judged to be the best skill (thus, nicknames are sticky)...
// other skills will have to attain the next level higher to be
// considered a better skill (thus, the first skill to reach level 27
// becomes the characters final nickname). -- bwr
void init_skill_order(void)
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; i++)
    {
        skill_type si = static_cast<skill_type>(i);

        const unsigned int i_points = you.skill_points[si]
                                      / species_apt_factor(si);

        you.skill_order[si] = 0;

        for (int j = SK_FIRST_SKILL; j < NUM_SKILLS; j++)
        {
            skill_type sj = static_cast<skill_type>(j);
            if (si == sj)
                continue;

            const unsigned int j_points = you.skill_points[sj]
                                          / species_apt_factor(sj);

            if (you.skills[sj] == you.skills[si]
                && (j_points > i_points
                    || (j_points == i_points && sj > si)))
            {
                you.skill_order[si]++;
            }
        }
    }
}

void calc_hp()
{
    you.hp_max = get_real_hp(true, false);
    if (you.species == SP_DJINNI)
        you.hp_max += get_real_mp(true);
    deflate_hp(you.hp_max, false);
}

void calc_mp()
{
    if (you.species == SP_DJINNI)
    {
        you.magic_points = you.max_magic_points = 0;
        return calc_hp();
    }

    you.max_magic_points = get_real_mp(true);
    you.magic_points = min(you.magic_points, you.max_magic_points);
    you.redraw_magic_points = true;
}

bool is_useless_skill(skill_type skill)
{
#if TAG_MAJOR_VERSION == 34
    if (skill == SK_STABBING || skill == SK_TRAPS)
        return true;
#endif
    return species_apt(skill) == -99;
}

bool is_harmful_skill(skill_type skill)
{
    return is_magic_skill(skill) && you_worship(GOD_TROG);
}

bool all_skills_maxed(bool inc_harmful)
{
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (you.skills[i] < 27 && you.can_train[i]
            && !is_useless_skill((skill_type) i)
            && (inc_harmful || !is_harmful_skill((skill_type) i)))
        {
            return false;
        }
    }

    return true;
}

int skill_bump(skill_type skill, int scale)
{
    int sk = you.skill_rdiv(skill, scale);
    return sk < 3 * scale ? sk * 2 : sk + 3 * scale;
}

// What aptitude value corresponds to doubled skill learning
// (i.e., old-style aptitude 50).
#define APT_DOUBLE 4

static float _apt_to_factor(int apt)
{
    return (1 / exp(log(2) * apt / APT_DOUBLE));
}

unsigned int skill_exp_needed(int lev, skill_type sk, species_type sp)
{
    const int exp[28] = { 0, 50, 150, 300, 500, 750,         // 0-5
                          1050, 1400, 1800, 2250, 2800,      // 6-10
                          3450, 4200, 5050, 6000, 7050,      // 11-15
                          8200, 9450, 10800, 12300, 13950,   // 16-20
                          15750, 17700, 19800, 22050, 24450, // 21-25
                          27000, 29750 };

    ASSERT_RANGE(lev, 0, 27 + 1);

    return exp[lev] * species_apt_factor(sk, sp);
}

int species_apt(skill_type skill, species_type species)
{
    static bool spec_skills_initialised = false;
    if (!spec_skills_initialised)
    {
        // Setup sentinel values to find errors more easily.
        const int sentinel = -20; // this gives cost 3200
        for (int sp = 0; sp < NUM_SPECIES; ++sp)
            for (int sk = 0; sk < NUM_SKILLS; ++sk)
                _spec_skills[sp][sk] = sentinel;
        for (unsigned i = 0; i < ARRAYSZ(species_skill_aptitudes); ++i)
        {
            const species_skill_aptitude &ssa(species_skill_aptitudes[i]);
            ASSERT(_spec_skills[ssa.species][ssa.skill] == sentinel);
            _spec_skills[ssa.species][ssa.skill] = ssa.aptitude;
        }
        spec_skills_initialised = true;
    }

    return _spec_skills[species][skill];
}

float species_apt_factor(skill_type sk, species_type sp)
{
    return _apt_to_factor(species_apt(sk, sp));
}

static vector<skill_type> _get_crosstrain_skills(skill_type sk)
{
    vector<skill_type> ret;

    switch (sk)
    {
    case SK_SHORT_BLADES:
        ret.push_back(SK_LONG_BLADES);
        return ret;
    case SK_LONG_BLADES:
        ret.push_back(SK_SHORT_BLADES);
        return ret;
    case SK_AXES:
    case SK_STAVES:
        ret.push_back(SK_POLEARMS);
        ret.push_back(SK_MACES_FLAILS);
        return ret;
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
        ret.push_back(SK_AXES);
        ret.push_back(SK_STAVES);
        return ret;
    case SK_SLINGS:
        ret.push_back(SK_THROWING);
        return ret;
    case SK_THROWING:
        ret.push_back(SK_SLINGS);
        return ret;
    default:
        return ret;
    }
}

// This threshold is in tenths of a skill point.
#define CROSSTRAIN_THRESHOLD 1

float crosstrain_bonus(skill_type sk)
{
    int bonus = 1;

    vector<skill_type> crosstrain_skills = _get_crosstrain_skills(sk);

    for (unsigned int i = 0; i < crosstrain_skills.size(); ++i)
        if (you.skill(crosstrain_skills[i], 10, true)
            >= you.skill(sk, 10, true) + CROSSTRAIN_THRESHOLD)
        {
            bonus *= 2;
        }

    return bonus;
}

bool crosstrain_other(skill_type sk, bool show_zero)
{
    vector<skill_type> crosstrain_skills = _get_crosstrain_skills(sk);

    for (unsigned int i = 0; i < crosstrain_skills.size(); ++i)
        if (you.skill(crosstrain_skills[i], 10, true)
            <= you.skill(sk, 10, true) - CROSSTRAIN_THRESHOLD
           && (you.skills[crosstrain_skills[i]] > 0 || show_zero))
        {
            return true;
        }

    return false;
}

static skill_type _get_opposite(skill_type sk)
{
    switch (sk)
    {
    case SK_FIRE_MAGIC  : return SK_ICE_MAGIC;   break;
    case SK_ICE_MAGIC   : return SK_FIRE_MAGIC;  break;
    case SK_AIR_MAGIC   : return SK_EARTH_MAGIC; break;
    case SK_EARTH_MAGIC : return SK_AIR_MAGIC;   break;
    default: return SK_NONE;
    }
}

static int _skill_elemental_preference(skill_type sk, int scale)
{
    const skill_type sk2 = _get_opposite(sk);
    if (sk2 == SK_NONE)
        return 0;
    return (you.skill(sk, scale) - you.skill(sk2, scale));
}

int elemental_preference(spell_type spell, int scale)
{
    skill_set skill_list;
    spell_skills(spell, skill_list);
    int preference = 0;
    for (skill_set_iter it = skill_list.begin(); it != skill_list.end(); ++it)
        preference += _skill_elemental_preference(*it, scale);
    return preference;
}

/*
 * Compare skill levels
 *
 * It compares the level of 2 skills, and breaks ties by using skill order.
 *
 * @param sk1 First skill.
 * @param sk2 Second skill.
 * @return Whether first skill is higher than second skill.
 */
static bool _compare_skills(skill_type sk1, skill_type sk2)
{
    if (is_invalid_skill(sk1))
        return false;
    else if (is_invalid_skill(sk2))
        return true;
    else
        return (you.skill(sk1, 10, true) > you.skill(sk2, 10, true)
                || you.skill(sk1, 10, true) == you.skill(sk2, 10, true)
                   && you.skill_order[sk1] < you.skill_order[sk2]);
}

bool is_antitrained(skill_type sk)
{
    skill_type opposite = _get_opposite(sk);
    if (opposite == SK_NONE || you.skills[sk] >= 27)
        return false;

    return _compare_skills(opposite, sk) && you.skills[opposite];
}

bool antitrain_other(skill_type sk, bool show_zero)
{
    skill_type opposite = _get_opposite(sk);
    if (opposite == SK_NONE)
        return false;

    return ((you.skills[opposite] > 0 || show_zero) && you.skills[sk] > 0
            && you.skills[opposite] < 27 && _compare_skills(sk, opposite));
}

void dump_skills(string &text)
{
    for (uint8_t i = 0; i < NUM_SKILLS; i++)
    {
        int real = you.skill((skill_type)i, 10, true);
        int cur  = you.skill((skill_type)i, 10);
        if (real > 0 || (!you.auto_training && you.train[i] > 0))
        {
            text += make_stringf(" %c Level %.*f%s %s\n",
                                 real == 270       ? 'O' :
                                 !you.can_train[i] ? ' ' :
                                 you.train[i] == 2 ? '*' :
                                 you.train[i]      ? '+' :
                                                     '-',
                                 real == 270 ? 0 : 1,
                                 real * 0.1,
                                 real != cur
                                     ? make_stringf("(%.*f)",
                                           cur == 270 ? 0 : 1,
                                           cur * 0.1).c_str()
                                     : "",
                                 skill_name(static_cast<skill_type>(i)));
        }
    }
}

int skill_transfer_amount(skill_type sk)
{
    ASSERT(!is_invalid_skill(sk));
    if (you.skill_points[sk] < 1000)
        return you.skill_points[sk] - skill_exp_needed(1, sk);
    else
        return max<int>(1000, you.skill_points[sk] / 2);
}

// Transfer skill points from one skill to another (Ashenzari transfer
// knowledge ability). If simu, it just simulates the transfer and don't
// change anything. It returns the new level of tsk.
int transfer_skill_points(skill_type fsk, skill_type tsk, int skp_max,
                          bool simu, bool boost)
{
    ASSERT(!is_invalid_skill(fsk) && !is_invalid_skill(tsk));

    const int penalty = 90; // 10% XP penalty
    int total_skp_lost   = 0; // skill points lost in fsk.
    int total_skp_gained = 0; // skill points gained in tsk.
    int fsk_level = you.skills[fsk];
    int tsk_level = you.skills[tsk];
    int fsk_points = you.skill_points[fsk];
    int tsk_points = you.skill_points[tsk];
    int fsk_ct_points = you.ct_skill_points[fsk];
    int tsk_ct_points = you.ct_skill_points[tsk];

    if (!simu && you.ct_skill_points[fsk] > 0)
        dprf("ct_skill_points[%s]: %d", skill_name(fsk), you.ct_skill_points[fsk]);

    // We need to transfer by small steps and updating skill levels each time
    // so that cross/anti-training are handled properly.
    while (total_skp_lost < skp_max
           && (simu || total_skp_lost < (int)you.transfer_skill_points))
    {
        int skp_lost = min(20, skp_max - total_skp_lost);
        int skp_gained = skp_lost * penalty / 100;

        float ct_bonus = crosstrain_bonus(tsk);
        if (ct_bonus > 1 && fsk != tsk)
        {
            skp_gained *= ct_bonus;
            you.ct_skill_points[tsk] += (1 - 1 / ct_bonus) * skp_gained;
        }
        else if (is_antitrained(tsk))
            skp_gained /= ANTITRAIN_PENALTY;

        ASSERT(you.skill_points[fsk] > you.ct_skill_points[fsk]);

        int ct_penalty = skp_lost * you.ct_skill_points[fsk]
                          / (you.skill_points[fsk] - you.ct_skill_points[fsk]);
        ct_penalty = min<int>(ct_penalty, you.ct_skill_points[fsk]);
        you.ct_skill_points[fsk] -= ct_penalty;
        skp_lost += ct_penalty;

        if (!simu)
        {
            skp_lost = min<int>(skp_lost, you.transfer_skill_points
                                          - total_skp_lost);
        }

        total_skp_lost += skp_lost;
        change_skill_points(fsk, -skp_lost, false);

        // If reducing fighting would reduce your maxHP to 0 or below,
        // we cancel the last step and end the transfer.
        if (fsk == SK_FIGHTING && get_real_hp(false, true) <= 0)
        {
            change_skill_points(fsk, skp_lost, false);
            total_skp_lost -= skp_lost;
            if (!simu)
                you.transfer_skill_points = total_skp_lost;
            break;
        }

        total_skp_gained += skp_gained;

        if (fsk != tsk)
        {
            change_skill_points(tsk, skp_gained, false);
            if (you.skills[tsk] == 27)
                break;
        }
    }

    int new_level = you.skill(tsk, 10, !boost);
    // Restore the level
    you.skills[fsk] = fsk_level;
    you.skills[tsk] = tsk_level;

    if (simu)
    {
        you.skill_points[fsk] = fsk_points;
        you.skill_points[tsk] = tsk_points;
        you.ct_skill_points[fsk] = fsk_ct_points;
        you.ct_skill_points[tsk] = tsk_ct_points;
    }
    else
    {
        // Perform the real level up
        check_skill_level_change(fsk);
        check_skill_level_change(tsk);
        if ((int)you.transfer_skill_points < total_skp_lost)
            you.transfer_skill_points = 0;
        else
            you.transfer_skill_points -= total_skp_lost;

        dprf("skill %s lost %d points", skill_name(fsk), total_skp_lost);
        dprf("skill %s gained %d points", skill_name(tsk), total_skp_gained);
        if (you.ct_skill_points[fsk] > 0)
            dprf("ct_skill_points[%s]: %d", skill_name(fsk), you.ct_skill_points[fsk]);

        if (you.transfer_skill_points == 0 || you.skills[tsk] == 27)
            ashenzari_end_transfer(true);
        else
            dprf("%d skill points left to transfer", you.transfer_skill_points);
    }
    return new_level;
}

void skill_state::save()
{
    can_train          = you.can_train;
    skills             = you.skills;
    train              = you.train;
    training           = you.training;
    skill_points       = you.skill_points;
    ct_skill_points    = you.ct_skill_points;
    skill_cost_level   = you.skill_cost_level;
    skill_order        = you.skill_order;
    auto_training      = you.auto_training;
    exp_available      = you.exp_available;
    total_experience   = you.total_experience;
    get_all_manual_charges(manual_charges);
    for (int i = 0; i < NUM_SKILLS; i++)
    {
        real_skills[i] = you.skill((skill_type)i, 10, true);
        changed_skills[i] = you.skill((skill_type)i, 10);
    }
}

void skill_state::restore_levels()
{
    you.skills                      = skills;
    you.skill_points                = skill_points;
    you.ct_skill_points             = ct_skill_points;
    you.skill_cost_level            = skill_cost_level;
    you.skill_order                 = skill_order;
    you.exp_available               = exp_available;
    you.total_experience            = total_experience;
    set_all_manual_charges(manual_charges);
}

void skill_state::restore_training()
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
        if (you.skills[i] < 27)
            you.train[i] = train[i];

    you.can_train                   = can_train;
    you.auto_training               = auto_training;
    reset_training();
}

// Sanitize skills after an upgrade, racechange, etc.
void fixup_skills()
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (is_useless_skill(sk))
            you.skill_points[i] = 0;
        you.skill_points[i] = min(you.skill_points[i], skill_exp_needed(27, sk));
        check_skill_level_change(sk);
    }
    init_can_train();

    if (you.exp_available >= calc_skill_cost(you.skill_cost_level))
        skill_menu(SKMF_EXPERIENCE);
}

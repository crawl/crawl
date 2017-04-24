#include "AppHdr.h"

#include "god-conduct.h"

#include "fight.h"
#include "god-abil.h" // ru sac key
#include "god-item.h" // is_*_spell
#include "god-wrath.h"
#include "libutil.h"
#include "message.h"
#include "monster.h"
#include "mon-util.h"
#include "religion.h"
#include "state.h"
#include "stringutil.h" // uppercase_first

#include <functional>

// Forward declarations.
static bool _god_likes_killing(const monster& victim);

/////////////////////////////////////////////////////////////////////
// god_conduct_trigger

god_conduct_trigger::god_conduct_trigger(
    conduct_type c, int pg, bool kn, const monster* vict)
  : conduct(c), pgain(pg), known(kn), enabled(true), victim(nullptr)
{
    if (vict)
    {
        victim.reset(new monster);
        *(victim.get()) = *vict;
    }
}

void god_conduct_trigger::set(conduct_type c, int pg, bool kn,
                              const monster* vict)
{
    conduct = c;
    pgain = pg;
    known = kn;
    victim.reset(nullptr);
    if (vict)
    {
        victim.reset(new monster);
        *victim.get() = *vict;
    }
}

god_conduct_trigger::~god_conduct_trigger()
{
    if (enabled && conduct != NUM_CONDUCTS)
        did_god_conduct(conduct, pgain, known, victim.get());
}

static const char *conducts[] =
{
    "",
    "Evil", "Holy", "Attack Holy", "Attack Neutral",
    "Attack Friend", "Friend Died",
    "Kill Living", "Kill Undead", "Kill Demon", "Kill Natural Evil",
    "Kill Unclean", "Kill Chaotic", "Kill Wizard", "Kill Priest",
    "Kill Holy", "Kill Fast", "Banishment",
    "Spell Memorise", "Spell Cast", "Spell Practise",
    "Cannibalism", "Eat Souled Being",
    "Deliberate Mutation", "Cause Glowing", "Use Unclean",
    "Use Chaos", "Desecrate Orcish Remains",
    "Kill Slime", "Kill Plant", "Was Hasty", "Corpse Violation",
    "Carrion Rot", "Souled Friend Died", "Attack In Sanctuary",
    "Kill Artificial", "Exploration", "Desecrate Holy Remains", "Seen Monster",
    "Fire", "Kill Fiery", "Sacrificed Love", "Channel", "Hurt Foe",
};
COMPILE_CHECK(ARRAYSZ(conducts) == NUM_CONDUCTS);

/**
 * Change piety & add penance in response to a conduct.
 *
 * @param piety_change      The change in piety (+ or -) the conduct caused.
 * @param piety_denom       ???
 * @param penance           Penance caused by the conduct.
 * @param thing_done        The conduct in question. Used for debug info.
 */
static void _handle_piety_penance(int piety_change, int piety_denom,
                                  int penance, conduct_type thing_done)
{
    const int old_piety = you.piety;
#ifndef DEBUG_DIAGNOSTICS
    UNUSED(thing_done);
    UNUSED(conducts);
    UNUSED(old_piety);
#endif

    if (piety_change > 0)
        gain_piety(piety_change, piety_denom);
    else
        dock_piety(div_rand_round(-piety_change, piety_denom), penance);

    // don't announce exploration piety unless you actually got a boost
    if ((piety_change || penance)
        && thing_done != DID_EXPLORATION || old_piety != you.piety)
    {

        dprf("conduct: %s; piety: %d (%+d/%d); penance: %d (%+d)",
             conducts[thing_done],
             you.piety, piety_change, piety_denom,
             you.penance[you.religion], penance);

    }
}

/**
 * Whether good gods that you folow are offended by you attacking a specific
 * holy monster.
 *
 * @param victim    The holy in question. (May be nullptr.)
 * @return          Whether DID_ATTACK_HOLY applies.
 */
static bool _attacking_holy_matters(const monster* victim)
{
    // Don't penalise the player for killing holies unless they were once
    // neutral, or were no-reward (e.g. created by wrath).
    return !victim
            || testbits(victim->flags, MF_NO_REWARD)
            || testbits(victim->flags, MF_WAS_NEUTRAL);
}

#if (__GNUC__ * 100 + __GNUC_MINOR__ < 408) && !defined(__clang__)
// g++ 4.7 incorrectly treats a function<> initialised from a null function
// pointer as non-empty.
typedef bool (*valid_victim_t)(const monster *);
typedef void (*special_piety_t)(int &piety, int &denom, const monster* victim);
#else
// But g++ 5.x seems to have problems converting lambdas into function pointers?
typedef function<bool (const monster *)> valid_victim_t;
typedef function<void (int &piety, int &denom, const monster* victim)>
    special_piety_t;
#endif

/// A definition of the way in which a god dislikes a conduct being taken.
struct dislike_response
{
    /// Description on god desc screen.
    const char* desc;
    /// Whether the god "strongly" dislikes doing this.
    bool really_dislike;
    /// Loss in piety for triggering this conduct; multiplied by 'level'.
    int piety_factor;
    /// Penance for triggering this conduct; multiplied by 'level'.
    int penance_factor;
    /// Something your god says when you accidentally triggered the conduct.
    /// Implies that unknowingly triggering the conduct is forgiven.
    const char *forgiveness_message;
    /// Something your god says when you trigger this conduct. May be nullptr.
    const char *message;
    /// A function that checks the victim of the conduct to see if the conduct
    /// should actually, really apply to it. If nullptr, all victims are valid.
    valid_victim_t valid_victim;
    /// A flat decrease to penance, after penance_factor is applied.
    int penance_offset;

    /// Apply this response to a given conduct, severity level, and victim.
    /// @param victim may be null.
    void operator()(conduct_type thing_done, int level, bool known,
                    const monster *victim)
    {
        // if the conduct filters on affected monsters, & the relevant monster
        // isn't valid, don't trigger the conduct's consequences.
        if (valid_victim && !valid_victim(victim))
            return;

        god_acting gdact;

        // If the player didn't have a way to know they were going to trigger
        // the conduct, and the god cares, print a message & bail.
        if (!known && forgiveness_message)
        {
            simple_god_message(forgiveness_message);
            return;
        }

        // trigger the actual effects of the conduct.

        // weird hack (to prevent spam?)
        if (you_worship(GOD_ZIN) && thing_done == DID_CAUSE_GLOWING)
        {
            static int last_glowing_lecture = -1;
            if (!level)
            {
                simple_god_message(" is not enthusiastic about the "
                                   "mutagenic glow surrounding you.");
            }
            else if (last_glowing_lecture != you.num_turns)
            {
                last_glowing_lecture = you.num_turns;
                // Increase contamination within yellow glow.
                simple_god_message(" does not appreciate the extra "
                                   "mutagenic glow surrounding you!");
            }
        }

        // a message, if we have one...
        if (message)
            simple_god_message(message);

        // ...and piety/penance.
        const int piety_loss = max(0, piety_factor * level);
        const int penance = max(0, penance_factor * level
                                   + penance_offset);
        _handle_piety_penance(-piety_loss, 1, penance, thing_done);
    }
};

/// Good gods', and Beogh's, response to cannibalism.
static const dislike_response RUDE_CANNIBALISM_RESPONSE = {
    "you perform cannibalism", true,
    5, 3, nullptr, " expects more respect for your departed relatives."
};

/// Zin and Ely's responses to desecrating holy remains.
static const dislike_response GOOD_DESECRATE_HOLY_RESPONSE = {
    "you desecrate holy remains", true,
    1, 1, nullptr, " expects more respect for holy creatures!"
};

/// Zin and Ely's responses to evil actions. TODO: parameterize & merge w/TSO
static const dislike_response GOOD_EVIL_RESPONSE = {
    "you use evil magic or items", true,
    1, 1, " forgives your inadvertent evil act, just this once."
};

/// Zin and Ely's responses to the player attacking holy creatures.
static const dislike_response GOOD_ATTACK_HOLY_RESPONSE = {
    "you attack non-hostile holy beings", true,
    1, 1, nullptr, nullptr, _attacking_holy_matters
};

/// Good gods' response to the player killing holy creatures.
static const dislike_response GOOD_KILL_HOLY_RESPONSE = {
    "you kill non-hostile holy beings", true,
    3, 3, nullptr, nullptr, _attacking_holy_matters
};

/// TSO and Ely's response to the player attacking neutral monsters.
static const dislike_response GOOD_ATTACK_NEUTRAL_RESPONSE = {
    "you attack neutral beings", true,
    1, 1, " forgives your inadvertent attack on a neutral, just this once."
};

/// Various gods' response to attacking a pal.
static dislike_response _on_attack_friend(const char* desc)
{
    return
    {
        desc, true,
        1, 3, " forgives your inadvertent attack on an ally, just this once.",
        nullptr, [] (const monster* victim) -> bool {
            dprf("hates friend : %d", god_hates_attacking_friend(you.religion, *victim));
            return god_hates_attacking_friend(you.religion, *victim);
        }
    };
}

/// Fedhas's response to a friend(ly plant) dying.
static dislike_response _on_fedhas_friend_death(const char* desc)
{
    return
    {
        desc, false,
        1, 0, nullptr, nullptr, [] (const monster* victim) -> bool {
            // ballistomycetes are penalized separately.
            return victim && fedhas_protects(*victim)
            && victim->mons_species() != MONS_BALLISTOMYCETE;
        }
    };
}

typedef map<conduct_type, dislike_response> peeve_map;

/// a per-god map of conducts to that god's angry reaction to those conducts.
static peeve_map divine_peeves[] =
{
    // GOD_NO_GOD
    peeve_map(),
    // GOD_ZIN,
    {
        { DID_CANNIBALISM, RUDE_CANNIBALISM_RESPONSE },
        { DID_ATTACK_HOLY, GOOD_ATTACK_HOLY_RESPONSE },
        { DID_KILL_HOLY, GOOD_KILL_HOLY_RESPONSE },
        { DID_DESECRATE_HOLY_REMAINS, GOOD_DESECRATE_HOLY_RESPONSE },
        { DID_EVIL, GOOD_EVIL_RESPONSE },
        { DID_ATTACK_FRIEND, _on_attack_friend("you attack allies") },
        { DID_ATTACK_NEUTRAL, {
            "you attack neutral beings", false,
            1, 0,
            " forgives your inadvertent attack on a neutral, just this once."
        } },
        { DID_ATTACK_IN_SANCTUARY, {
            "you attack monters in a sanctuary", false,
            1, 1
        } },
        { DID_UNCLEAN, {
            "you use unclean or chaotic magic or items", true,
            1, 1, " forgives your inadvertent unclean act, just this once."
        } },
        { DID_CHAOS, {
            "you polymorph monsters", true,
            1, 1, " forgives your inadvertent chaotic act, just this once."
        } },
        { DID_DELIBERATE_MUTATING, {
            "you deliberately mutate or transform yourself", true,
            1, 0, " forgives your inadvertent chaotic act, just this once."
        } },
        { DID_DESECRATE_SOULED_BEING, {
            "you butcher sentient beings", true,
            5, 3
        } },
        { DID_CAUSE_GLOWING, { nullptr, false, 1 } },
    },
    // GOD_SHINING_ONE,
    {
        { DID_CANNIBALISM, RUDE_CANNIBALISM_RESPONSE },
        { DID_ATTACK_HOLY, {
            "you attack non-hostile holy beings", true,
            1, 2, nullptr, nullptr, _attacking_holy_matters
        } },
        { DID_KILL_HOLY, GOOD_KILL_HOLY_RESPONSE },
        { DID_DESECRATE_HOLY_REMAINS, {
            "you desecrate holy remains", true,
            1, 2, nullptr, " expects more respect for holy creatures!"
        } },
        { DID_EVIL, {
            "you use evil magic or items", true,
            1, 2, " forgives your inadvertent evil act, just this once."
        } },
        { DID_ATTACK_NEUTRAL, GOOD_ATTACK_NEUTRAL_RESPONSE },
        { DID_ATTACK_FRIEND, _on_attack_friend("you attack allies") },
    },
    // GOD_KIKUBAAQUDGHA,
    peeve_map(),
    // GOD_YREDELEMNUL,
    {
        { DID_HOLY, {
            "you use holy magic or items", true,
            1, 2, " forgives your inadvertent holy act, just this once."
        } },
    },
    // GOD_XOM,
    peeve_map(),
    // GOD_VEHUMET,
    peeve_map(),
    // GOD_OKAWARU,
    {
        { DID_ATTACK_FRIEND, _on_attack_friend("you attack allies") },
    },
    // GOD_MAKHLEB,
    peeve_map(),
    // GOD_SIF_MUNA,
    peeve_map(),
    // GOD_TROG,
    {
        { DID_SPELL_MEMORISE, {
            "you memorise spells", true,
            10, 10
        } },
        { DID_SPELL_CASTING, {
            "you attempt to cast spells", true,
            1, 5,
        } },
        { DID_SPELL_PRACTISE, {
            "you train magic skills", true,
            1, 0, nullptr, " doesn't appreciate your training magic!"
        } },
    },
    // GOD_NEMELEX_XOBEH,
    peeve_map(),
    // GOD_ELYVILON,
    {
        { DID_CANNIBALISM, RUDE_CANNIBALISM_RESPONSE },
        { DID_ATTACK_HOLY, GOOD_ATTACK_HOLY_RESPONSE },
        { DID_KILL_HOLY, GOOD_KILL_HOLY_RESPONSE },
        { DID_DESECRATE_HOLY_REMAINS, GOOD_DESECRATE_HOLY_RESPONSE },
        { DID_EVIL, GOOD_EVIL_RESPONSE },
        { DID_ATTACK_NEUTRAL, GOOD_ATTACK_NEUTRAL_RESPONSE },
        { DID_ATTACK_FRIEND, _on_attack_friend("you attack allies") },
        { DID_KILL_LIVING, {
            "you kill living things while asking for your life to be spared",
            true,
            1, 2, nullptr, " does not appreciate your shedding blood"
                            " when asking for salvation!",
            [] (const monster*) -> bool {
                // Killing is only disapproved of during prayer.
                return you.duration[DUR_LIFESAVING] != 0;
            }
        } },
    },
    // GOD_LUGONU,
    peeve_map(),
    // GOD_BEOGH,
    {
        { DID_CANNIBALISM, RUDE_CANNIBALISM_RESPONSE },
        { DID_DESECRATE_ORCISH_REMAINS, { "you desecrate orcish remains", true, 1 } },
        { DID_ATTACK_FRIEND, _on_attack_friend("you attack allied orcs") },
    },
    // GOD_JIYVA,
    {
        { DID_KILL_SLIME, {
            "you kill slimes", true,
            1, 2, nullptr, nullptr, [] (const monster* victim) -> bool {
                return victim && !victim->is_shapeshifter();
            }
        } },
        { DID_ATTACK_NEUTRAL, {
            nullptr, true,
            1, 1, nullptr, nullptr, [] (const monster* victim) -> bool {
                return victim
                    && mons_is_slime(*victim) && !victim->is_shapeshifter();
            }
        } },
        { DID_ATTACK_FRIEND, _on_attack_friend("you attack fellow slimes") },
    },
    // GOD_FEDHAS,
    {
        { DID_CORPSE_VIOLATION, {
            "you use necromancy on corpses, chunks or skeletons", true,
            1, 1, " forgives your inadvertent necromancy, just this once."
        } },
        { DID_KILL_PLANT, {
            "you destroy plants", false,
            1, 0
        } },
        { DID_ATTACK_FRIEND, _on_attack_friend(nullptr) },

        { DID_FRIEND_DIED, _on_fedhas_friend_death("allied flora die") },
        { DID_SOULED_FRIEND_DIED, _on_fedhas_friend_death(nullptr) },
    },
    // GOD_CHEIBRIADOS,
    {
        { DID_HASTY, {
            "you hasten yourself or others", true,
            1, 1, " forgives your accidental hurry, just this once.",
            " thinks you should slow down.", nullptr, -5
        } },
    },
    // GOD_ASHENZARI,
    peeve_map(),
    // GOD_DITHMENOS,
    {
        { DID_FIRE, {
            "you use fiery magic or items", false,
            1, 1, " forgives your accidental fire-starting, just this once.",
            " does not appreciate your starting fires!", nullptr, -5
        } },
    },
    // GOD_GOZAG,
    peeve_map(),
    // GOD_QAZLAL,
    peeve_map(),
    // GOD_RU,
    peeve_map(),
    // GOD_PAKELLAS
    {
        { DID_CHANNEL, {
            "you channel magical energy", true,
            1, 1,
        } },
    },
    // GOD_USKAYAW,
    peeve_map(),
    // GOD_HEPLIAKLQANA,
    peeve_map(),
    // GOD_WU_JIAN,
    peeve_map(),
};

string get_god_dislikes(god_type which_god)
{
    // Return early for the special cases.
    if (which_god == GOD_NO_GOD || which_god == GOD_XOM)
        return "";

    string text;
    vector<string> dislikes;        // Piety loss
    vector<string> really_dislikes; // Penance

    for (const auto& entry : divine_peeves[which_god])
    {
        if (entry.second.desc)
        {
            if (entry.second.really_dislike)
                really_dislikes.emplace_back(entry.second.desc);
            else
                dislikes.emplace_back(entry.second.desc);
        }
    }

    if (which_god == GOD_CHEIBRIADOS)
        really_dislikes.emplace_back("use unnaturally quick items");

    if (dislikes.empty() && really_dislikes.empty())
        return "";

    if (!dislikes.empty())
    {
        text += uppercase_first(god_name(which_god));
        text += " dislikes it when ";
        text += comma_separated_line(dislikes.begin(), dislikes.end(),
                                     " or ", ", ");
        text += ".";

        if (!really_dislikes.empty())
            text += " ";
    }

    if (!really_dislikes.empty())
    {
        text += uppercase_first(god_name(which_god));
        text += " strongly dislikes it when ";
        text += comma_separated_line(really_dislikes.begin(),
                                     really_dislikes.end(),
                                     " or ", ", ");
        text += ".";
    }

    return text;
}

/// A definition of the way in which a god likes a conduct being taken.
struct like_response
{
    /// How to describe this conduct on the god description screen.
    const char* desc;
    /// Whether the god should be described as "especially" liking it.
    bool really_like;
    /** Gain in piety for triggering this conduct; added to calculated denom.
     *
     * This number is usually negative. In that case, the maximum piety gain
     * is one point, and the chance of *not* getting that point is:
     *    -piety_bonus/(piety_denom_bonus + level - you.xl/xl_denom)
     * (omitting the you.xl term if xl_denom is zero)
     */
    int piety_bonus;
    /// Divider for piety gained by this conduct; added to 'level'.
    int piety_denom_bonus;
    /// Degree to which your XL modifies piety gained. If zero, do not
    /// modify piety by XL; otherwise divide player XL by this value.
    int xl_denom;
    /// Something your god says when you trigger this conduct. May be nullptr.
    const char *message;
    /// Special-case code for weird likes. May modify piety bonus/denom, or
    /// may have other side effects. If nullptr, doesn't trigger, ofc.
    special_piety_t special;

    /// Apply this response to a given conduct, severity level, and victim.
    /// @param victim may be null.
    void operator()(conduct_type thing_done, int level, bool /*known*/,
                    const monster *victim)
    {
        // if the conduct filters on affected monsters, & the relevant monster
        // isn't valid, don't trigger the conduct's consequences.
        if (victim && !_god_likes_killing(*victim))
            return;

        god_acting gdact;

        if (message)
            simple_god_message(message);

        // this is all very strange, but replicates legacy behavior.
        // See the comment on piety_bonus above.
        int denom = piety_denom_bonus + level;
        if (xl_denom)
            denom -= you.get_experience_level() / xl_denom;

        int gain = denom + piety_bonus;

        // handle weird special cases
        // may modify gain/denom
        if (special)
            special(gain, denom, victim);

        _handle_piety_penance(max(0, gain), max(1, denom), 0, thing_done);
    }
};

/**
 * The piety bonus that is given for killing monsters of the appropriate
 * holiness.
 *
 * Gets slotted into a very strange equation. It's weird.
 */
static int _piety_bonus_for_holiness(mon_holy_type holiness)
{
    if (holiness & (MH_NATURAL | MH_PLANT | MH_NONLIVING))
        return -6;
    else if (holiness & MH_UNDEAD)
        return -5;
    else if (holiness & MH_DEMONIC)
        return -4;
    else if (holiness & MH_HOLY)
        return -3;
    else
        die("unknown holiness type; can't give a bonus");
}

/**
 * Generate an appropriate kill response (piety gain scaling, message, &c),
 * for gods that like killing this sort of thing.
 *
 * @param holiness      The holiness of the relevant type of monsters.
 * @param god_is_good   Whether this is a good god.
 *                      (They don't scale piety with XL in the same way...?)
 * @param special       A special-case function.
 * @return              An appropropriate like_response.
 */
static like_response _on_kill(const char* desc, mon_holy_type holiness,
                              bool god_is_good = false,
                              special_piety_t special = nullptr,
                              bool really_like = false)
{
    return {
        desc,
        really_like,
        _piety_bonus_for_holiness(holiness),
        18,
        god_is_good ? 0 : 2,
        " accepts your kill.",
        special
    };
}

/// Response for gods that like killing the living.
static const like_response KILL_LIVING_RESPONSE =
    _on_kill("you kill living beings", MH_NATURAL);

/// Response for non-good gods that like killing (?) undead.
static const like_response KILL_UNDEAD_RESPONSE =
    _on_kill("you destroy the undead", MH_UNDEAD);

/// Response for non-good gods that like killing (?) demons.
static const like_response KILL_DEMON_RESPONSE =
    _on_kill("you kill demons", MH_DEMONIC);

/// Response for non-good gods that like killing (?) holies.
static const like_response KILL_HOLY_RESPONSE =
    _on_kill("you kill holy beings", MH_HOLY);

/// Response for non-good gods that like killing (?) nonliving enemies.
static const like_response KILL_NONLIVING_RESPONSE =
    _on_kill("you destroy nonliving beings", MH_NONLIVING);

// Note that holy deaths are special - they're always noticed...
// If you or any friendly kills one, you'll get the credit/blame.

static like_response okawaru_kill(const char* desc)
{
    return
    {
        desc, false,
        0, 0, 0, nullptr, [] (int &piety, int &denom, const monster* victim)
        {
            piety = get_fuzzied_monster_difficulty(*victim);
            dprf("fuzzied monster difficulty: %4.2f", piety * 0.01);
            denom = 550;

            if (piety > 3200)
            {
                mprf(MSGCH_GOD, you.religion,
                     "<white>%s is honoured by your kill.</white>",
                     uppercase_first(god_name(you.religion)).c_str());
            }
            else if (piety > 9) // might still be miniscule
                simple_god_message(" accepts your kill.");
        }
    };
}

static const like_response EXPLORE_RESPONSE = {
    "you explore the world", false,
    0, 0, 0, nullptr,
    [] (int &piety, int &denom, const monster* /*victim*/)
    {
        // piety = denom = level at the start of the function
        piety = 14;
    }
};


typedef map<conduct_type, like_response> like_map;

/// a per-god map of conducts to piety rewards given by that god.
static like_map divine_likes[] =
{
    // GOD_NO_GOD
    like_map(),
    // GOD_ZIN,
    {
        { DID_KILL_UNCLEAN, _on_kill("you kill unclean or chaotic beings", MH_DEMONIC, true) },
        { DID_KILL_CHAOTIC, _on_kill(nullptr, MH_DEMONIC, true) },
    },
    // GOD_SHINING_ONE,
    {
        { DID_KILL_UNDEAD, _on_kill("you kill the undead", MH_UNDEAD, true) },
        { DID_KILL_DEMON, _on_kill("you kill demons", MH_DEMONIC, true) },
        { DID_KILL_NATURAL_EVIL, _on_kill("you kill evil beings", MH_DEMONIC, true) },
        { DID_SEE_MONSTER, {
            "you encounter other hostile creatures", false,
            0, 0, 0, nullptr, [] (int &piety, int &denom, const monster* victim)
            {
                // don't give piety for seeing things we get piety for killing.
                if (victim && victim->evil())
                    return;

                const int level = denom; // also = piety
                denom = level / 2 + 6 - you.experience_level / 4;
                piety = denom - 4;
            }
        } },
    },
    // GOD_KIKUBAAQUDGHA,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
    },
    // GOD_YREDELEMNUL,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, _on_kill("you kill holy beings", MH_HOLY, false,
                                  [](int &piety, int &denom,
                                     const monster* victim)
            {
                piety *= 2;
                simple_god_message(" appreciates your killing of a holy being.");
            },
            true
        ) },
        { DID_KILL_NONLIVING, {
            "you destroy nonliving beings", true, 3, 18, 2, " accepts your kill."
        } },
    },
    // GOD_XOM,
    like_map(),
    // GOD_VEHUMET,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
    },
    // GOD_OKAWARU,
    {
        { DID_KILL_LIVING, okawaru_kill("you kill living beings") },
        { DID_KILL_UNDEAD, okawaru_kill("you destroy the undead") },
        { DID_KILL_DEMON, okawaru_kill("you kill demons") },
        { DID_KILL_HOLY, okawaru_kill("you kill holy beings") },
        { DID_KILL_NONLIVING, okawaru_kill("you destroy nonliving beings") },
    },
    // GOD_MAKHLEB,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
    },
    // GOD_SIF_MUNA,
    {
        { DID_KILL_LIVING, _on_kill("you kill living beings", MH_NATURAL, false,
                                  [](int &piety, int &denom,
                                     const monster* victim)
            {
                denom *= 3;
            }
        ) },
        { DID_KILL_UNDEAD, _on_kill("you destroy the undead", MH_UNDEAD, false,
                                  [](int &piety, int &denom,
                                     const monster* victim)
            {
                denom *= 3;
            }
        ) },
        { DID_KILL_DEMON, _on_kill("you kill demons", MH_DEMONIC, false,
                                  [](int &piety, int &denom,
                                     const monster* victim)
            {
                denom *= 3;
            }
        ) },
        { DID_KILL_HOLY, _on_kill("you kill holy beings", MH_HOLY, false,
                                  [](int &piety, int &denom,
                                     const monster* victim)
            {
                denom *= 3;
            }
        ) },
        { DID_KILL_NONLIVING, _on_kill("you destroy nonliving beings",
                                       MH_NONLIVING, false,
                                       [](int &piety, int &denom,
                                          const monster* victim)
            {
                denom *= 3;
            }
        ) },
        { DID_SPELL_PRACTISE, {
            "you train your various spell casting skills", true,
            0, 0, 0, nullptr,
            [] (int &piety, int &denom, const monster* /*victim*/)
            {
                // piety = denom = level at the start of the function
                denom = 6;
            }
        } },
    },
    // GOD_TROG,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
        { DID_KILL_WIZARD, {
            "you kill wizards and other users of magic", true,
            -6, 10, 0, " appreciates your killing of a magic user."
        } },
    },
    // GOD_NEMELEX_XOBEH,
    {
        { DID_EXPLORATION, EXPLORE_RESPONSE },
    },
    // GOD_ELYVILON,
    {
        { DID_EXPLORATION, {
            "you explore the world", false,
            0, 0, 0, nullptr,
            [] (int &piety, int &denom, const monster* /*victim*/)
            {
                // piety = denom = level at the start of the function
                piety = 20;
            }
        } },
    },
    // GOD_LUGONU,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
        { DID_BANISH, {
            "you banish creatures to the Abyss", false,
            -6, 18, 2, " claims a new guest."
        } },
    },
    // GOD_BEOGH,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
        { DID_KILL_PRIEST, {
            "you kill the priests of other religions", true,
            -6, 18, 0, " appreciates your killing of a heretic priest."
        } },
    },
    // GOD_JIYVA,
    like_map(),
    // GOD_FEDHAS,
    {
        { DID_ROT_CARRION, {
            "corpses rot away", false, 0, 0, 0, " appreciates ongoing decay."
        } },
    },
    // GOD_CHEIBRIADOS,
    {
        { DID_KILL_FAST, {
            "you kill fast things, relative to your speed", false,
            -6, 18, 2, nullptr,
            [] (int &piety, int &/*denom*/, const monster* victim)
            {
                const int speed_delta =
                    cheibriados_monster_player_speed_delta(*victim);
                dprf("Chei DID_KILL_FAST: %s speed delta: %d",
                     victim->name(DESC_PLAIN, true).c_str(),
                     speed_delta);

                if (speed_delta > 0 && x_chance_in_y(speed_delta, 12))
                {
                    simple_god_message(" thoroughly appreciates the change of pace.");
                    piety *= 2;
                }
                else
                    simple_god_message(" appreciates the change of pace.");
            }
        } }
    },
    // GOD_ASHENZARI,
    {
        { DID_EXPLORATION, {
            "you explore the world (preferably while bound by curses)", false,
            0, 0, 0, nullptr,
            [] (int &piety, int &denom, const monster* /*victim*/)
            {
                const int level = denom; // also = piety
                const int base_gain = 8; // base gain per dungeon level
                // levels: x1, x1.25, x1.5, x1.75, x2
                piety = base_gain + base_gain * you.bondage_level / 4;
                denom = level;
            }
        } },
    },
    // GOD_DITHMENOS,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
        { DID_KILL_FIERY, {
            "you kill beings that bring fire to the dungeon", true,
            -6, 10, 0, " appreciates your extinguishing a source of fire."
        } },
    },
    // GOD_GOZAG,
    like_map(),
    // GOD_QAZLAL,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
    },
    // GOD_RU,
    {
        { DID_EXPLORATION, {
            nullptr, false, 0, 0, 0, nullptr,
            [] (int &piety, int &denom, const monster* /*victim*/)
            {
                piety = 0;
                denom = 1;

                ASSERT(you.props.exists(RU_SACRIFICE_PROGRESS_KEY));

                if (one_chance_in(100))
                    you.props[RU_SACRIFICE_PROGRESS_KEY].get_int()++;
            }
        } },
    },
    // GOD_PAKELLAS,
    {
        { DID_KILL_LIVING, _on_kill("you kill living beings", MH_NATURAL, false,
                                  [](int &piety, int &denom,
                                     const monster* victim)
            {
                piety *= 4;
                denom *= 3;
            }
        ) },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
    },
    // GOD_USKAYAW
    {
        { DID_HURT_FOE, {
            "you hurt your foes; however, effects that cause damage over "
            "time do not interest Uskayaw", true, 1, 1, 0, nullptr,
            [] (int &piety, int &denom, const monster* /*victim*/)
            {
                denom = 1;
            }
        } },
    },
    // GOD_HEPLIAKLQANA
    {
        { DID_EXPLORATION, EXPLORE_RESPONSE },
    },
    // GOD_WU_JIAN
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_KILL_NONLIVING, KILL_NONLIVING_RESPONSE },
    },
};

/**
 * Will your god give you piety for killing the given monster, assuming that
 * its death triggers a conduct that the god sometimes likes?
 */
static bool _god_likes_killing(const monster& victim)
{
    return !god_hates_attacking_friend(you.religion, victim);
}

static void _handle_your_gods_response(conduct_type thing_done, int level,
                                       bool known, const monster* victim)
{
    // Lucy gives no piety in Abyss. :(
    // XXX: make this not a hack...? (or remove it?)
    if (you_worship(GOD_LUGONU) && player_in_branch(BRANCH_ABYSS))
        return;

    COMPILE_CHECK(ARRAYSZ(divine_peeves) == NUM_GODS);
    COMPILE_CHECK(ARRAYSZ(divine_likes) == NUM_GODS);

    // If your god disliked the action, evaluate its response.
    if (auto peeve = map_find(divine_peeves[you.religion], thing_done))
        (*peeve)(thing_done, level, known, victim);

    // If your god liked the action, evaluate its response.
    if (auto like = map_find(divine_likes[you.religion], thing_done))
        (*like)(thing_done, level, known, victim);
}

/**
 * Handle god conducts triggered by killing a monster.
 *
 * @param thing_done        The conduct in question.
 * @param victim            The deceased. (RIP.)
 */
void did_kill_conduct(conduct_type thing_done, const monster &victim)
{
    did_god_conduct(thing_done, victim.get_experience_level(), true, &victim);
}

// This function is the merger of done_good() and naughty().
void did_god_conduct(conduct_type thing_done, int level, bool known,
                     const monster* victim)
{
    ASSERT(!crawl_state.game_is_arena());

    _handle_your_gods_response(thing_done, level, known, victim);
}

// A Beogh worshipper zapping an orc with lightning might cause it to become a
// follower on the first hit, and the second hit would be against a friendly
// orc. Don't cause penance in this case.
static set<mid_t> _first_attack_conduct;
static set<mid_t> _first_attack_was_friendly;

void god_conduct_turn_start()
{
    _first_attack_conduct.clear();
    _first_attack_was_friendly.clear();
}

void set_attack_conducts(god_conduct_trigger conduct[3], const monster* mon,
                         bool known)
{
    ASSERT(mon);  // TODO: change to const monster &mon
    const mid_t mid = mon->mid;

    if (mon->friendly())
    {
        if (_first_attack_conduct.find(mid) == _first_attack_conduct.end()
            || _first_attack_was_friendly.find(mid)
               != _first_attack_was_friendly.end())
        {
            conduct[0].set(DID_ATTACK_FRIEND, 5, known, mon);
            _first_attack_was_friendly.insert(mid);
        }
    }
    else if (mon->neutral())
        conduct[0].set(DID_ATTACK_NEUTRAL, 5, known, mon);

    if (mon->is_holy() && !mon->is_illusion())
        conduct[2].set(DID_ATTACK_HOLY, mon->get_experience_level(), known, mon);

    _first_attack_conduct.insert(mid);
}

void enable_attack_conducts(god_conduct_trigger conduct[3])
{
    for (int i = 0; i < 3; ++i)
        conduct[i].enabled = true;
}

void disable_attack_conducts(god_conduct_trigger conduct[3])
{
    for (int i = 0; i < 3; ++i)
        conduct[i].enabled = false;
}

string get_god_likes(god_type which_god)
{
    if (which_god == GOD_NO_GOD || which_god == GOD_XOM)
        return "";

    string text = uppercase_first(god_name(which_god));
    vector<string> likes;
    vector<string> really_likes;

    // Unique/unusual piety gain methods first.
    switch (which_god)
    {
    case GOD_TROG:
        likes.emplace_back("you destroy spellbooks via the <w>a</w> command");
        break;
    case GOD_JIYVA:
        likes.emplace_back("you sacrifice items by allowing slimes to consume "
                           "them");
        break;
    case GOD_GOZAG:
        likes.emplace_back("you collect gold");
        break;
    case GOD_RU:
        likes.emplace_back("you make personal sacrifices");
        break;
    case GOD_ZIN:
        likes.emplace_back("you donate money");
        break;
    default:
        break;
    }


    for (const auto& entry : divine_likes[which_god])
    {
        if (entry.second.desc)
        {
            if (entry.second.really_like)
                really_likes.emplace_back(entry.second.desc);
            else
                likes.emplace_back(entry.second.desc);
        }
    }

    if (likes.empty() && really_likes.empty())
        text += " doesn't like anything? This is a bug; please report it.";
    else
    {
        if (!likes.empty())
        {
            text += " likes it when ";
            text += comma_separated_line(likes.begin(), likes.end());
            text += ".";
            if (!really_likes.empty())
            {
                text += " ";
                text += uppercase_first(god_name(which_god));
            }
        }

        if (!really_likes.empty())
        {
            text += " especially likes it when ";
            text += comma_separated_line(really_likes.begin(),
                                         really_likes.end());
            text += ".";
        }
    }

    return text;
}

bool god_hates_cannibalism(god_type god)
{
    return divine_peeves[god].count(DID_CANNIBALISM);
}

/**
 * Handle god conducts triggered by hurting a monster. Currently set up to only
 * account for Uskayaw's use pattern; if anyone else uses it, add a second case.
 *
 * @param thing_done        The conduct in question.
 * @param victim            The victim being harmed.
 * @param damage_done       The amount of damage done.
 */
void did_hurt_conduct(conduct_type thing_done,
                      const monster &victim,
                      int damage_done)
{
    // Currently only used by Uskayaw; initially planned to use god conduct
    // logic more heavily, but the god seems to need something different.

    if (you_worship(GOD_USKAYAW))
    {
        // Give a "value" for the percent of the monster's hp done in damage,
        // scaled by the monster's threat level.11
        int value = random2(3) + sqr((mons_threat_level(victim) + 1) * 2)
                * damage_done / (victim.max_hit_points);

        you.props[USKAYAW_NUM_MONSTERS_HURT].get_int() += 1;
        you.props[USKAYAW_MONSTER_HURT_VALUE].get_int() += value;
    }
}

/**
 * Will this god definitely be upset if you cast this spell?
 *
 * This is as opposed to a likelihood, such as TSO's relationship with PArrow.
 * TODO: deduplicate with spl-cast.cc:_spellcasting_god_conduct
 *
 * @param spell the spell to be cast
 * @param god   the god to check against
 * @returns true if you will definitely lose piety/get penance/be excommunicated
 */
bool god_punishes_spell(spell_type spell, god_type god)
{
    if (map_find(divine_peeves[god], DID_SPELL_CASTING))
        return true;

    if (god_loathes_spell(spell, god))
        return true;

    if (map_find(divine_peeves[god], DID_EVIL)
        && (is_evil_spell(spell) || you.spellcasting_unholy()))
    {
        return true;
    }

    if (map_find(divine_peeves[god], DID_UNCLEAN) && is_unclean_spell(spell))
        return true;

    if (map_find(divine_peeves[god], DID_CHAOS) && is_chaotic_spell(spell))
        return true;

    // not is_hasty_spell: see spl-cast.cc:_spellcasting_god_conduct
    if (map_find(divine_peeves[god], DID_HASTY) && spell == SPELL_SWIFTNESS)
        return true;

    if (map_find(divine_peeves[god], DID_CORPSE_VIOLATION)
        && is_corpse_violating_spell(spell))
    {
        return true;
    }

    return false;
}

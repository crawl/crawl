#include "AppHdr.h"

#include "godconduct.h"

#include "fight.h"
#include "godwrath.h"
#include "libutil.h"
#include "message.h"
#include "monster.h"
#include "mon-util.h"
#include "religion.h"
#include "state.h"

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
    victim.reset(NULL);
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

#ifdef DEBUG_DIAGNOSTICS
static const char *conducts[] =
{
    "",
    "Necromancy", "Holy", "Unholy", "Attack Holy", "Attack Neutral",
    "Attack Friend", "Friend Died", "Unchivalric Attack",
    "Poison", "Kill Living", "Kill Undead",
    "Kill Demon", "Kill Natural Unholy", "Kill Natural Evil",
    "Kill Unclean", "Kill Chaotic", "Kill Wizard", "Kill Priest",
    "Kill Holy", "Kill Fast", "Undead Slave Kill Living",
    "Servant Kill Living", "Undead Slave Kill Undead",
    "Servant Kill Undead", "Undead Slave Kill Demon",
    "Servant Kill Demon", "Servant Kill Natural Unholy",
    "Servant Kill Natural Evil", "Undead Slave Kill Holy",
    "Servant Kill Holy", "Banishment", "Spell Memorise", "Spell Cast",
    "Spell Practise",
    "Drink Blood", "Cannibalism","Eat Souled Being",
    "Deliberate Mutation", "Cause Glowing", "Use Unclean",
    "Use Chaos", "Desecrate Orcish Remains", "Destroy Orcish Idol",
    "Kill Slime", "Kill Plant", "Servant Kill Plant",
    "Was Hasty", "Corpse Violation",
    "Souled Friend Died", "Servant Kill Unclean",
    "Servant Kill Chaotic", "Attack In Sanctuary",
    "Kill Artificial", "Undead Slave Kill Artificial",
    "Servant Kill Artificial", "Destroy Spellbook",
    "Exploration", "Desecrate Holy Remains", "Seen Monster",
    "Fire", "Kill Fiery", "Sacrificed Love"
};
#endif

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
#ifdef DEBUG_DIAGNOSTICS
    const int old_piety = you.piety;
#else
    UNUSED(thing_done);
#endif

    if (piety_change > 0)
        gain_piety(piety_change, piety_denom);
    else
        dock_piety(div_rand_round(-piety_change, piety_denom), penance);

#ifdef DEBUG_DIAGNOSTICS
    // don't announce exploration piety unless you actually got a boost
    if ((piety_change || penance)
        && thing_done != DID_EXPLORATION || old_piety != you.piety)
    {

        COMPILE_CHECK(ARRAYSZ(conducts) == NUM_CONDUCTS);
        dprf("conduct: %s; piety: %d (%+d/%d); penance: %d (%+d)",
             conducts[thing_done],
             you.piety, piety_change, piety_denom,
             you.penance[you.religion], penance);

    }
#endif

}

/**
 * Whether good gods that you folow are offended by you attacking a specific
 * holy monster.
 *
 * @param victim    The holy in question. (May be NULL.)
 * @return          Whether DID_ATTACK_HOLY applies.
 */
static bool _attacking_holy_matters(const monster* victim)
{
    // XXX: what does this do?
    return !victim
            || testbits(victim->flags, MF_NO_REWARD)
            || testbits(victim->flags, MF_WAS_NEUTRAL);
}




/// A definition of the way in which a god dislikes a conduct being taken.
struct dislike_response
{
    /// Loss in piety for triggering this conduct; multiplied by 'level'.
    int piety_factor;
    /// Penance for triggering this conduct; multiplied by 'level'.
    int penance_factor;
    /// Something your god says when you accidentally triggered the conduct.
    /// Implies that unknowingly triggering the conduct is forgiven.
    const char *forgiveness_message;
    /// Something your god says when you trigger this conduct. May be NULL.
    const char *message;
    /// A function that checks the victim of the conduct to see if the conduct
    /// should actually, really apply to it. If NULL, all victims are valid.
    bool (*valid_victim)(const monster* victim);
    /// A flat decrease to penance, after penance_factor is applied.
    int penance_offset;
};



/// Good gods' reaction to drinking blood.
static const dislike_response GOOD_BLOOD_RESPONSE = {
    2, 1, " forgives your inadvertent blood-drinking, just this once."
};

/// Good gods', and Beogh's, response to cannibalism.
static const dislike_response RUDE_CANNIBALISM_RESPONSE = {
    5, 3, NULL, " expects more respect for your departed relatives."
};

/// Zin and Ely's responses to desecrating holy remains.
static const dislike_response GOOD_DESECRATE_HOLY_RESPONSE = {
    1, 1, NULL, " expects more respect for holy creatures!"
};

/// Zin and Ely's responses to unholy actions & necromancy.
static const dislike_response GOOD_UNHOLY_RESPONSE = {
    1, 1, " forgives your inadvertent unholy act, just this once."
};

/// Zin and Ely's responses to the player attacking holy creatures.
static const dislike_response GOOD_ATTACK_HOLY_RESPONSE = {
    1, 1, NULL, NULL, _attacking_holy_matters
};

/// Good gods' response to the player killing holy creatures.
static const dislike_response GOOD_KILL_HOLY_RESPONSE = {
    3, 3, NULL, NULL, _attacking_holy_matters
};

/// TSO's response to the player stabbing or poisoning monsters.
static const dislike_response TSO_UNCHIVALRIC_RESPONSE = {
    1, 2, " forgives your inadvertent dishonourable attack, just"
              " this once.", NULL, [] (const monster* victim) {
        return !victim || !tso_unchivalric_attack_safe_monster(victim);
    }
};

/// TSO and Ely's response to the player attacking neutral monsters.
static const dislike_response GOOD_ATTACK_NEUTRAL_RESPONSE = {
    1, 1, " forgives your inadvertent attack on a neutral, just this once."
};

/// Various gods' response to attacking a pal.
static const dislike_response ATTACK_FRIEND_RESPONSE = {
    1, 3, " forgives your inadvertent attack on an ally, just this once.",
    NULL, [] (const monster* victim) {
        dprf("hates friend : %d", god_hates_attacking_friend(you.religion, victim));
        return god_hates_attacking_friend(you.religion, victim);
    }
};

/// Ely response to a friend dying.
static const dislike_response ELY_FRIEND_DEATH_RESPONSE = {
    1, 0, NULL, NULL, [] (const monster* victim) {
        // For everyone but Fedhas, plants are items not creatures,
        // and animated items are, well, items as well.
        return victim && !mons_is_object(victim->type)
                      && victim->holiness() != MH_PLANT
        // Converted allies (marked as TSOites) can be martyrs.
                      && victim->god == GOD_SHINING_ONE;
    }
};

/// Fedhas's response to a friend(ly plant) dying.
static const dislike_response FEDHAS_FRIEND_DEATH_RESPONSE = {
    1, 0, NULL, NULL, [] (const monster* victim) {
        // ballistomycetes are penalized separately.
        return victim && fedhas_protects(victim)
        && victim->mons_species() != MONS_BALLISTOMYCETE;
    }
};

typedef map<conduct_type, dislike_response> peeve_map;

/// a per-god map of conducts to that god's angry reaction to those conducts.
static peeve_map divine_peeves[] =
{
    // GOD_NO_GOD
    peeve_map(),
    // GOD_ZIN,
    {
        { DID_DRINK_BLOOD, GOOD_BLOOD_RESPONSE },
        { DID_CANNIBALISM, RUDE_CANNIBALISM_RESPONSE },
        { DID_ATTACK_HOLY, GOOD_ATTACK_HOLY_RESPONSE },
        { DID_KILL_HOLY, GOOD_KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_SERVANT, GOOD_KILL_HOLY_RESPONSE },
        { DID_DESECRATE_HOLY_REMAINS, GOOD_DESECRATE_HOLY_RESPONSE },
        { DID_NECROMANCY, GOOD_UNHOLY_RESPONSE },
        { DID_UNHOLY, GOOD_UNHOLY_RESPONSE },
        { DID_ATTACK_FRIEND, ATTACK_FRIEND_RESPONSE },
        { DID_ATTACK_NEUTRAL, {
            1, 0,
            " forgives your inadvertent attack on a neutral, just this once."
        } },
        { DID_ATTACK_IN_SANCTUARY, {
            1, 1
        } },
        { DID_UNCLEAN, {
            1, 1, " forgives your inadvertent unclean act, just this once."
        } },
        { DID_CHAOS, {
            1, 1, " forgives your inadvertent chaotic act, just this once."
        } },
        { DID_DELIBERATE_MUTATING, {
            1, 0, " forgives your inadvertent chaotic act, just this once."
        } },
        { DID_DESECRATE_SOULED_BEING, {
            5, 3
        } },
        { DID_CAUSE_GLOWING, { 1 } },
    },
    // GOD_SHINING_ONE,
    {
        { DID_DRINK_BLOOD, GOOD_BLOOD_RESPONSE },
        { DID_CANNIBALISM, RUDE_CANNIBALISM_RESPONSE },
        { DID_ATTACK_HOLY, {
            1, 2, NULL, NULL, _attacking_holy_matters
        } },
        { DID_KILL_HOLY, GOOD_KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_SERVANT, GOOD_KILL_HOLY_RESPONSE },
        { DID_DESECRATE_HOLY_REMAINS, {
            1, 2, NULL, " expects more respect for holy creatures!"
        } },
        { DID_NECROMANCY, {
            1, 2, " forgives your inadvertent unholy act, just this once."
        } },
        { DID_UNHOLY, {
            1, 2, " forgives your inadvertent unholy act, just this once."
        } },
        { DID_UNCHIVALRIC_ATTACK, TSO_UNCHIVALRIC_RESPONSE },
        { DID_POISON, TSO_UNCHIVALRIC_RESPONSE },
        { DID_ATTACK_NEUTRAL, GOOD_ATTACK_NEUTRAL_RESPONSE },
        { DID_ATTACK_FRIEND, ATTACK_FRIEND_RESPONSE },
    },
    // GOD_KIKUBAAQUDGHA,
    peeve_map(),
    // GOD_YREDELEMNUL,
    {
        { DID_HOLY, {
            1, 2, " forgives your inadvertent holy act, just this once."
        } },
    },
    // GOD_XOM,
    peeve_map(),
    // GOD_VEHUMET,
    peeve_map(),
    // GOD_OKAWARU,
    {
        { DID_ATTACK_FRIEND, ATTACK_FRIEND_RESPONSE },
    },
    // GOD_MAKHLEB,
    peeve_map(),
    // GOD_SIF_MUNA,
    {
        { DID_DESTROY_SPELLBOOK, { 1 } },
    },
    // GOD_TROG,
    {
        { DID_SPELL_MEMORISE, {
            10, 10
        } },
        { DID_SPELL_CASTING, {
            1, 5,
        } },
        { DID_SPELL_PRACTISE, {
            1, 0, NULL, " doesn't appreciate your training magic!"
        } },
    },
    // GOD_NEMELEX_XOBEH,
    peeve_map(),
    // GOD_ELYVILON,
    {
        { DID_DRINK_BLOOD, GOOD_BLOOD_RESPONSE },
        { DID_CANNIBALISM, RUDE_CANNIBALISM_RESPONSE },
        { DID_ATTACK_HOLY, GOOD_ATTACK_HOLY_RESPONSE },
        { DID_KILL_HOLY, GOOD_KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_SERVANT, GOOD_KILL_HOLY_RESPONSE },
        { DID_DESECRATE_HOLY_REMAINS, GOOD_DESECRATE_HOLY_RESPONSE },
        { DID_NECROMANCY, GOOD_UNHOLY_RESPONSE },
        { DID_UNHOLY, GOOD_UNHOLY_RESPONSE },
        { DID_ATTACK_NEUTRAL, GOOD_ATTACK_NEUTRAL_RESPONSE },
        { DID_ATTACK_FRIEND, ATTACK_FRIEND_RESPONSE },
        { DID_FRIEND_DIED, ELY_FRIEND_DEATH_RESPONSE },
        { DID_SOULED_FRIEND_DIED, ELY_FRIEND_DEATH_RESPONSE },
        { DID_KILL_LIVING, {
            1, 2, NULL, " does not appreciate your shedding blood"
                            " when asking for salvation!",
            [] (const monster* _) {
                UNUSED(_);
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
        { DID_DESECRATE_ORCISH_REMAINS, { 1 } },
        { DID_DESTROY_ORCISH_IDOL, { 1, 3 } },
        { DID_ATTACK_FRIEND, ATTACK_FRIEND_RESPONSE },
    },
    // GOD_JIYVA,
    {
        { DID_KILL_SLIME, {
            1, 2, NULL, NULL, [] (const monster* victim) {
                return victim && !victim->is_shapeshifter();
            }
        } },
        { DID_ATTACK_NEUTRAL, {
            1, 1, NULL, NULL, [] (const monster* victim) {
                return victim
                    && mons_is_slime(victim) && !victim->is_shapeshifter();
            }
        } },
        { DID_ATTACK_FRIEND, ATTACK_FRIEND_RESPONSE },
    },
    // GOD_FEDHAS,
    {
        { DID_CORPSE_VIOLATION, {
            1, 1, " forgives your inadvertent necromancy, just this once."
        } },
        { DID_KILL_PLANT, {
            1, 0
        } },
        { DID_PLANT_KILLED_BY_SERVANT, {
            1, 0
        } },
        { DID_ATTACK_FRIEND, ATTACK_FRIEND_RESPONSE },
        { DID_FRIEND_DIED, FEDHAS_FRIEND_DEATH_RESPONSE },
        { DID_SOULED_FRIEND_DIED, FEDHAS_FRIEND_DEATH_RESPONSE },
    },
    // GOD_CHEIBRIADOS,
    {
        { DID_HASTY, {
            1, 1, " forgives your accidental hurry, just this once.",
            " thinks you should slow down.", NULL, -5
        } },
    },
    // GOD_ASHENZARI,
    peeve_map(),
    // GOD_DITHMENOS,
    {
        { DID_FIRE, {
            1, 1, " forgives your accidental fire-starting, just this once.",
            " does not appreciate your starting fires!", NULL, -5
        } },
    },
    // GOD_GOZAG,
    peeve_map(),
    // GOD_QAZLAL,
    peeve_map(),
    // GOD_RU,
    peeve_map(),
};


/**
 * Handle Dithmenos's piety scaling based on piety rank.
 *
 * @param[in,out] piety    Piety gain is modified & output into this.
 * @param[in,out] denom    Piety gain denominator is modified & output here.
 * @param victim           Unused.
 */
static void _dithmenos_kill(int &piety, int &denom, const monster* /*victim*/)
{
    // Full gains at full piety down to 2/3 at 6* piety.
    // (piety_rank starts at 1, not 0.)
    piety *= 25 - piety_rank();
    denom *= 24;
}

/// A definition of the way in which a god likes a conduct being taken.
struct like_response
{
    /// Gain in piety for triggering this conduct; added to calculated denom.
    int piety_bonus;
    /// Divider for piety gained by this conduct; added to 'level'.
    int piety_denom_bonus;
    /// Degree to which your XL modifies piety gained.
    int xl_denom;
    /// Something your god says when you trigger this conduct. May be NULL.
    const char *message;
    /// Special-case code for weird likes. May modify piety bonus/denom, or
    /// may have other side effects. If NULL, doesn't trigger, ofc.
    void (*special)(int &piety, int &denom, const monster* victim);
};

/**
 * The piety bonus that is given for killing monsters of the appropriate
 * holiness.
 *
 * Gets slotted into a very strange equation. It's weird.
 */
static int _piety_bonus_for_holiness(mon_holy_type holiness)
{
    switch (holiness)
    {
        case MH_NATURAL:
        case MH_PLANT:
            return -6;
        case MH_UNDEAD:
            return -5;
        case MH_DEMONIC:
            return -4;
        case MH_HOLY:
        case MH_NONLIVING: // hi, yred
            return -3;
        default:
            die("unknown holiness type; can't give a bonus");
    }
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
static like_response _on_kill(mon_holy_type holiness, bool god_is_good = false,
                             void (*special)(int &piety, int &denom,
                                             const monster* victim) = NULL)
{
    like_response response = {
        _piety_bonus_for_holiness(holiness),
        18,
        god_is_good ? 0 : 2,
        " accepts your kill.",
        special
    };
    return response;
}

/// Response for gods that like killing the living.
static const like_response KILL_LIVING_RESPONSE = _on_kill(MH_NATURAL);

/// Response for non-good gods that like killing (?) undead.
static const like_response KILL_UNDEAD_RESPONSE = _on_kill(MH_UNDEAD);

/// Response for non-good gods that like killing (?) demons.
static const like_response KILL_DEMON_RESPONSE = _on_kill(MH_DEMONIC);

/// Response for non-good gods that like killing (?) holies.
static const like_response KILL_HOLY_RESPONSE =  _on_kill(MH_HOLY);

/// Response for TSO/Zin when you kill (most) things they hate.
static const like_response GOOD_KILL_RESPONSE = _on_kill(MH_DEMONIC, true);
// Note that holy deaths are special - they're always noticed...
// If you or any friendly kills one, you'll get the credit/blame.

/// Response for gods that like your undead slaves killing holies.
/// Also, Yred uses this for killing artificials.
static const like_response UNDEAD_KILL_HOLY_RESPONSE = {
    -3, 18, 0, " accepts your slave's kill."
};

/// Response for gods that like your servants killing holies.
static const like_response SERVANT_KILL_HOLY_RESPONSE = {
    -3, 18, 0, " accepts your collateral kill."
};

/// Response for gods that like your undead slaves killing non-holy things.
static const like_response UNDEAD_KILL_RESPONSE = {
    -6, 10, 3, " accepts your slave's kill."
};

/// Response for gods that like your servants killing non-holy things.
static const like_response SERVANT_KILL_RESPONSE = {
    -6, 10, 3, " accepts your collateral kill."
};

/// Response for TSO/Zin when your servants kill things they hate.
static const like_response GOOD_COLLATERAL_RESPONSE = {
    -6, 10, 0, " accepts your collateral kill."
};

static const like_response OKAWARU_KILL = {
    0, 0, 0, NULL, [] (int &piety, int &denom, const monster* victim)
    {
        piety = get_fuzzied_monster_difficulty(victim);
        dprf("fuzzied monster difficulty: %4.2f", piety * 0.01);
        denom = 600;

        if (piety > 3200)
            simple_god_message(" appreciates your kill.");
        else if (piety > 9) // might still be miniscule
            simple_god_message(" accepts your kill.");
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
        { DID_KILL_UNCLEAN, GOOD_KILL_RESPONSE },
        { DID_KILL_CHAOTIC, GOOD_KILL_RESPONSE },
        { DID_UNCLEAN_KILLED_BY_SERVANT, GOOD_COLLATERAL_RESPONSE },
        { DID_CHAOTIC_KILLED_BY_SERVANT, GOOD_COLLATERAL_RESPONSE },
    },
    // GOD_SHINING_ONE,
    {
        { DID_KILL_UNDEAD, _on_kill(MH_UNDEAD, true) },
        { DID_KILL_DEMON, GOOD_KILL_RESPONSE },
        { DID_KILL_NATURAL_UNHOLY, GOOD_KILL_RESPONSE },
        { DID_KILL_NATURAL_EVIL, GOOD_KILL_RESPONSE },
        { DID_UNDEAD_KILLED_BY_SERVANT, GOOD_COLLATERAL_RESPONSE },
        { DID_DEMON_KILLED_BY_SERVANT, GOOD_COLLATERAL_RESPONSE },
        { DID_NATURAL_UNHOLY_KILLED_BY_SERVANT, GOOD_COLLATERAL_RESPONSE },
        { DID_NATURAL_EVIL_KILLED_BY_SERVANT, GOOD_COLLATERAL_RESPONSE },
        { DID_SEE_MONSTER, {
            0, 0, 0, NULL, [] (int &piety, int &denom, const monster* victim)
            {
                // don't give piety for seeing things we get piety for killing.
                if (victim && (victim->is_evil() || victim->is_unholy()))
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
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_HOLY_RESPONSE },
        { DID_LIVING_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_DEMON_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
    },
    // GOD_YREDELEMNUL,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_HOLY, _on_kill(MH_HOLY, false,
                                  [](int &piety, int &denom,
                                     const monster* victim)
            {
                piety *= 2;
                simple_god_message(" appreciates your killing of a holy being.");
            }
        ) },
        { DID_KILL_ARTIFICIAL, _on_kill(MH_NONLIVING, false) },
        { DID_HOLY_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_HOLY_RESPONSE },
        { DID_ARTIFICIAL_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_HOLY_RESPONSE },
        { DID_LIVING_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
    },
    // GOD_XOM,
    like_map(),
    // GOD_VEHUMET,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
    },
    // GOD_OKAWARU,
    {
        { DID_KILL_LIVING, OKAWARU_KILL },
        { DID_KILL_UNDEAD, OKAWARU_KILL },
        { DID_KILL_DEMON, OKAWARU_KILL },
        { DID_KILL_HOLY, OKAWARU_KILL },
    },
    // GOD_MAKHLEB,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_SERVANT, SERVANT_KILL_HOLY_RESPONSE },
        { DID_LIVING_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_LIVING_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_UNDEAD_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_UNDEAD_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_DEMON_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_DEMON_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
    },
    // GOD_SIF_MUNA,
    {
        { DID_SPELL_PRACTISE, {
            0, 0, 0, NULL, [] (int &piety, int &denom, const monster* victim)
            {
                UNUSED(victim);
                // piety = denom = level at the start of the function
                denom = 4;
            }
        } },
    },
    // GOD_TROG,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_SERVANT, SERVANT_KILL_HOLY_RESPONSE },
        { DID_LIVING_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_DEMON_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_KILL_WIZARD, {
            -6, 10, 0, " appreciates your killing of a magic user."
        } },
    },
    // GOD_NEMELEX_XOBEH,
    {
        { DID_EXPLORATION, {
            0, 0, 0, NULL, [] (int &piety, int &denom, const monster* victim)
            {
                UNUSED(victim);
                // piety = denom = level at the start of the function
                piety = 14;
            }
        } },
    },
    // GOD_ELYVILON,
    like_map(),
    // GOD_LUGONU,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_SERVANT, SERVANT_KILL_HOLY_RESPONSE },
        { DID_LIVING_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_LIVING_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_UNDEAD_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_UNDEAD_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_DEMON_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_DEMON_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_BANISH, {
            -6, 18, 2, " claims a new guest."
        } },
    },
    // GOD_BEOGH,
    {
        { DID_KILL_LIVING, KILL_LIVING_RESPONSE },
        { DID_KILL_UNDEAD, KILL_UNDEAD_RESPONSE },
        { DID_KILL_DEMON, KILL_DEMON_RESPONSE },
        { DID_KILL_HOLY, KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_SERVANT, SERVANT_KILL_HOLY_RESPONSE },
        { DID_HOLY_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_HOLY_RESPONSE },
        { DID_LIVING_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_LIVING_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_UNDEAD_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_UNDEAD_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_DEMON_KILLED_BY_UNDEAD_SLAVE, UNDEAD_KILL_RESPONSE },
        { DID_DEMON_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_KILL_PRIEST, {
            -6, 10, 0, " appreciates your killing of a heretic priest."
        } },
    },
    // GOD_JIYVA,
    like_map(),
    // GOD_FEDHAS,
    like_map(),
    // GOD_CHEIBRIADOS,
    {
        { DID_KILL_FAST, {
            -6, 18, 2, NULL, [] (int &piety, int &denom, const monster* victim)
            {
                UNUSED(denom);

                const int speed_delta =
                    cheibriados_monster_player_speed_delta(victim);
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
            0, 0, 0, NULL, [] (int &piety, int &denom, const monster* victim)
            {
                UNUSED(victim);

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
        { DID_KILL_LIVING, _on_kill(MH_NATURAL, false, _dithmenos_kill) },
        { DID_KILL_UNDEAD, _on_kill(MH_UNDEAD, false, _dithmenos_kill) },
        { DID_KILL_DEMON, _on_kill(MH_DEMONIC, false, _dithmenos_kill) },
        { DID_KILL_HOLY, _on_kill(MH_HOLY, false, _dithmenos_kill) },
        { DID_KILL_FIERY, {
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
        { DID_HOLY_KILLED_BY_SERVANT, SERVANT_KILL_HOLY_RESPONSE },
        { DID_LIVING_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_UNDEAD_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
        { DID_DEMON_KILLED_BY_SERVANT, SERVANT_KILL_RESPONSE },
    },
    // GOD_RU,
    {
        { DID_EXPLORATION, {
            0, 0, 0, NULL, [] (int &piety, int &denom, const monster* victim)
            {
                piety = 0;
                denom = 1;
                UNUSED(victim);

                ASSERT(you.props.exists("ru_progress_to_next_sacrifice"));
                ASSERT(you.props.exists("available_sacrifices"));

                const int available_sacrifices =
                    you.props["available_sacrifices"].get_vector().size();

                if (!available_sacrifices && one_chance_in(100))
                    you.props["ru_progress_to_next_sacrifice"].get_int()++;
            }
        } },
    },
};


/**
 * Will your god give you piety for killing the given monster, assuming that
 * its death triggers a conduct that the god sometimes likes?
 */
static bool _god_likes_killing(const monster* victim)
{
    if (you_worship(GOD_DITHMENOS)
        && mons_class_flag(victim->type, M_SHADOW))
    {
        return false;
    }

    return !god_hates_attacking_friend(you.religion, victim);
}


static void _handle_your_gods_response(conduct_type thing_done, int level,
                                       bool known, const monster* victim)
{
    // Lucy gives no piety in Abyss. :(
    // XXX: make this not a hack...? (or remove it?)
    if (you_worship(GOD_LUGONU) && player_in_branch(BRANCH_ABYSS))
        return;

    // handle new-style conduct responses

    // check for dislikes
    COMPILE_CHECK(ARRAYSZ(divine_peeves) == NUM_GODS);
    if (divine_peeves[you.religion].count(thing_done))
    {
        const dislike_response peeve = divine_peeves[you.religion][thing_done];

        // if the conduct filters on affected monsters, & the relevant monster
        // isn't valid, don't trigger the conduct's consequences.
        if (peeve.valid_victim && !peeve.valid_victim(victim))
            return;

        god_acting gdact;

        // If the player didn't have a way to know they were going to trigger
        // the conduct, and the god cares, print a message & bail.
        if (!known && peeve.forgiveness_message)
        {
            simple_god_message(peeve.forgiveness_message);
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
        if (peeve.message)
            simple_god_message(peeve.message);

        // ...and piety/penance.
        const int piety_loss = max(0, peeve.piety_factor * level);
        const int penance = max(0, peeve.penance_factor * level
                                   + peeve.penance_offset);
        _handle_piety_penance(-piety_loss, 1, penance, thing_done);
    }

    //check for likes
    COMPILE_CHECK(ARRAYSZ(divine_likes) == NUM_GODS);
    if (divine_likes[you.religion].count(thing_done))
    {
        const like_response like = divine_likes[you.religion][thing_done];

        // if the conduct filters on affected monsters, & the relevant monster
        // isn't valid, don't trigger the conduct's consequences.
        if (victim && !_god_likes_killing(victim))
            return;

        god_acting gdact;

        if (like.message)
            simple_god_message(like.message);

        // this is all very strange, but replicates legacy behavior.
        int denom = like.piety_denom_bonus + level;
        if (like.xl_denom)
            denom -= you.get_experience_level() / like.xl_denom;

        int gain = denom + like.piety_bonus;

        // handle weird special cases
        // may modify gain/denom
        if (like.special)
            like.special(gain, denom, victim);

        _handle_piety_penance(max(0, gain), max(1, denom), 0, thing_done);
    }
}

// a sad and shrunken function.
static void _handle_other_gods_response(conduct_type thing_done)
{
    if (thing_done == DID_DESTROY_ORCISH_IDOL)
        beogh_idol_revenge();
}


// This function is the merger of done_good() and naughty().
// Returns true if god was interested (good or bad) in conduct.
void did_god_conduct(conduct_type thing_done, int level, bool known,
                     const monster* victim)
{
    ASSERT(!crawl_state.game_is_arena());

    _handle_your_gods_response(thing_done, level, known, victim);
    _handle_other_gods_response(thing_done);
}

// These two arrays deal with the situation where a beam hits a non-fleeing
// monster, the monster starts to flee because of the damage, and then the
// beam bounces and hits the monster again.  If the monster wasn't fleeing
// when the beam started then hits from bounces shouldn't count as
// unchivalric attacks, but if the first hit from the beam *was* unchivalrous
// then all the bounces should count as unchivalrous as well.
//
// Also do the same sort of check for harming a friendly monster,
// since a Beogh worshipper zapping an orc with lightning might cause it to
// become a follower on the first hit, and the second hit would be
// against a friendly orc.
static FixedBitVector<MAX_MONSTERS> _first_attack_conduct;
static FixedBitVector<MAX_MONSTERS> _first_attack_was_unchivalric;
static FixedBitVector<MAX_MONSTERS> _first_attack_was_friendly;

void god_conduct_turn_start()
{
    _first_attack_conduct.reset();
    _first_attack_was_unchivalric.reset();
    _first_attack_was_friendly.reset();
}

void set_attack_conducts(god_conduct_trigger conduct[3], const monster* mon,
                         bool known)
{
    const unsigned int midx = mon->mindex();

    if (mon->friendly())
    {
        if (!_first_attack_conduct[midx]
            || _first_attack_was_friendly[midx])
        {
            conduct[0].set(DID_ATTACK_FRIEND, 5, known, mon);
            _first_attack_was_friendly.set(midx);
        }
    }
    else if (mon->neutral())
        conduct[0].set(DID_ATTACK_NEUTRAL, 5, known, mon);

    if (find_stab_type(&you, mon) != STAB_NO_STAB
        && (!_first_attack_conduct[midx]
            || _first_attack_was_unchivalric[midx]))
    {
        conduct[1].set(DID_UNCHIVALRIC_ATTACK, 4, known, mon);
        _first_attack_was_unchivalric.set(midx);
    }

    if (mon->is_holy() && !mon->is_illusion())
        conduct[2].set(DID_ATTACK_HOLY, mon->get_experience_level(), known, mon);

    _first_attack_conduct.set(midx);
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

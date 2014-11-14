/**
 * @file
 * @brief God-granted abilities.
**/

#include "AppHdr.h"

#include "godabil.h"

#include <queue>
#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "branch.h"
#include "butcher.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "dgn-overview.h"
#include "directn.h"
#include "dungeon.h"
#include "effects.h"
#include "english.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "godblessing.h"
#include "godcompanions.h"
#include "goditem.h"
#include "hiscores.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "losglobal.h"
#include "macro.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mutation.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "place.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "skill_menu.h"
#include "spl-book.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "sprint.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "throw.h"
#ifdef USE_TILE
 #include "tiledef-main.h"
#endif
#include "traps.h"
#include "viewchar.h"
#include "view.h"

// Load the sacrifice_def definition and the sac_data array.
#include "sacrifice-data.h"

static void _zin_saltify(monster* mon);

string zin_recite_text(const int seed, const int prayertype, int step)
{
    // 'prayertype':
    // This is in enum.h; there are currently four prayers.

    // 'step':
    // -1: We're either starting or stopping, so we just want the passage name.
    // 2/1/0: That many rounds are left. So, if step = 2, we want to show the passage #1/3.

    // That's too confusing, so:

    if (step > -1)
    {
        step = abs(step-3);
        if (step > 3)
            step = 0;
    }

    // We change it to turn 1, turn 2, turn 3.

    // 'trits':
    // To have deterministic passages we need to store a random seed.
    // Ours consists of an array of trinary bits.

    // Yes, really.

    const int trits[7] = { seed % 3, (seed / 3) % 3, (seed / 9) % 3,
                           (seed / 27) % 3, (seed / 81) % 3, (seed / 243) % 3,
                           (seed / 729) % 3};
    const int chapter = 1 + trits[0] + trits[1] * 3 + trits[2] * 9;
    const int verse = 1 + trits[3] + trits[4] * 3 + trits[5] * 9 + trits[6] * 27;

    string sinner_text[12] =
    {
        "hordes of the Abyss",
        "bastard children of Xom",
        "amorphous wretches",
        "fetid masses",
        "agents of filth",
        "squalid dregs",
        "unbelievers",
        "heretics",
        "guilty",
        "legions of the damned",
        "servants of Hell",
        "forces of darkness",
    };

    string sin_text[12] =
    {
        "chaotic",
        "discordant",
        "anarchic",
        "unclean",
        "impure",
        "contaminated",
        "unfaithful",
        "disloyal",
        "doubting",
        "profane",
        "blasphemous",
        "sacrilegious",
    };

    string long_sin_text[12] =
    {
        "chaos",
        "discord",
        "anarchy",
        "uncleanliness",
        "impurity",
        "contamination",
        "unfaithfulness",
        "disloyalty",
        "doubt",
        "profanity",
        "blasphemy",
        "sacrilege",
    };

    string virtue_text[12] =
    {
        "ordered",
        "harmonic",
        "lawful",
        "clean",
        "pure",
        "hygienic",
        "faithful",
        "loyal",
        "believing",
        "reverent",
        "pious",
        "obedient",
    };

    string long_virtue_text[12] =
    {
        "order",
        "harmony",
        "lawfulness",
        "cleanliness",
        "purity",
        "hygiene",
        "faithfulness",
        "loyalty",
        "belief",
        "reverence",
        "piety",
        "obedience",
    };

    string smite_text[9] =
    {
        "purify",
        "censure",
        "condemn",
        "strike down",
        "expel",
        "oust",
        "smite",
        "castigate",
        "rebuke",
    };

    string smitten_text[9] =
    {
        "purified",
        "censured",
        "condemned",
        "struck down",
        "expelled",
        "ousted",
        "smitten",
        "castigated",
        "rebuked",
    };

    string sinner = sinner_text[trits[3] + prayertype * 3];
    string sin[2] = {sin_text[trits[6] + prayertype * 3],
                     long_sin_text[trits[6] + prayertype * 3]};
    string virtue[2] = {virtue_text[trits[6] + prayertype * 3],
                        long_virtue_text[trits[6] + prayertype * 3]};
    string smite[2] = {smite_text[(trits[4] + trits[5] * 3)],
                       smitten_text[(trits[4] + trits[5] * 3)]};

    string turn[4] = {"This is only here because arrays start from 0.",
                      "Zin is a buggy god.", "Please report this.",
                      "This isn't right at all."};

    switch (chapter)
    {
        case 1:
            turn[1] = make_stringf("It was the word of Zin that there would not be %s...",
                                   sin[1].c_str());
            turn[2] = make_stringf("...and did the people not suffer until they had %s...",
                                   smite[1].c_str());
            turn[3] = make_stringf("...the %s, after which all was well?", sinner.c_str());
            break;
        case 2:
            turn[1] = make_stringf("The voice of Zin, pure and clear, did say that the %s...",
                                   sinner.c_str());
            turn[2] = make_stringf("...were not %s! And hearing this, the people rose up...",
                                   virtue[0].c_str());
            turn[3] = make_stringf("...and embraced %s, for they feared Zin's wrath.",
                                   virtue[1].c_str());
            break;
        case 3:
            turn[1] = make_stringf("Zin spoke of the doctrine of %s, and...",
                                   virtue[1].c_str());
            turn[2] = make_stringf("...saw the %s filled with fear, for they were...",
                                   sinner.c_str());
            turn[3] = make_stringf("...%s and knew Zin's wrath would come for them.",
                                   sin[0].c_str());
            break;
        case 4:
            turn[1] = make_stringf("And so Zin bade the %s to come before...",
                                   sinner.c_str());
            turn[2] = make_stringf("...the altar, that judgement might be passed...");
            turn[3] = make_stringf("...upon those who were not %s.", virtue[0].c_str());
            break;
        case 5:
            turn[1] = make_stringf("To the devout, Zin provideth. From the rest...");
            turn[2] = make_stringf("...ye %s, ye guilty...", sinner.c_str());
            turn[3] = make_stringf("...of %s, Zin taketh.", sin[1].c_str());
            break;
        case 6:
            turn[1] = make_stringf("Zin saw the %s of the %s, and...",
                                   sin[1].c_str(), sinner.c_str());
            turn[2] = make_stringf("...was displeased, for did the law not say that...");
            turn[3] = make_stringf("...those who did not become %s would be %s?",
                                   virtue[0].c_str(), smite[1].c_str());
            break;
        case 7:
            turn[1] = make_stringf("Zin said that %s shall be the law of the land, and...",
                                   virtue[1].c_str());
            turn[2] = make_stringf("...those who turn to %s will be %s. This was fair...",
                                   sin[1].c_str(), smite[1].c_str());
            turn[3] = make_stringf("...and just, and not a voice dissented.");
            break;
        case 8:
            turn[1] = make_stringf("Damned, damned be the %s and...", sinner.c_str());
            turn[2] = make_stringf("...all else who abandon %s! Let them...",
                                   virtue[1].c_str());
            turn[3] = make_stringf("...be %s by the jurisprudence of Zin!",
                                   smite[1].c_str());
            break;
        case 9:
            turn[1] = make_stringf("And Zin said to all in attendance, 'Which of ye...");
            turn[2] = make_stringf("...number among the %s? Come before me, that...",
                                   sinner.c_str());
            turn[3] = make_stringf("...I may %s you now for your %s!'",
                                   smite[0].c_str(), sin[1].c_str());
            break;
        case 10:
            turn[1] = make_stringf("Yea, I say unto thee, bring forth...");
            turn[2] = make_stringf("...the %s that they may know...", sinner.c_str());
            turn[3] = make_stringf("...the wrath of Zin, and thus be %s!",
                                   smite[1].c_str());
            break;
        case 11:
            turn[1] = make_stringf("In a great set of silver scales are weighed the...");
            turn[2] = make_stringf("...souls of the %s, and with their %s...",
                                   sinner.c_str(), sin[0].c_str());
            turn[3] = make_stringf("...ways, the balance hath tipped against them!");
            break;
        case 12:
            turn[1] = make_stringf("It is just that the %s shall be %s...",
                                   sinner.c_str(), smite[1].c_str());
            turn[2] = make_stringf("...in due time, for %s is what Zin has declared...",
                                   virtue[1].c_str());
            turn[3] = make_stringf("...the law of the land, and Zin's word is law!");
            break;
        case 13:
            turn[1] = make_stringf("Thus the people made the covenant of %s with...",
                                   virtue[1].c_str());
            turn[2] = make_stringf("...Zin, and all was good, for they knew that the...");
            turn[3] = make_stringf("...%s would trouble them no longer.", sinner.c_str());
            break;
        case 14:
            turn[1] = make_stringf("What of the %s? %s for their...",
                                   sinner.c_str(), uppercase_first(smite[1]).c_str());
            turn[2] = make_stringf("...%s they shall be! Zin will %s them again...",
                                   sin[1].c_str(), smite[0].c_str());
            turn[3] = make_stringf("...and again, and again!");
            break;
        case 15:
            turn[1] = make_stringf("And lo, the wrath of Zin did find...");
            turn[2] = make_stringf("...them wherever they hid, and the %s...",
                                   sinner.c_str());
            turn[3] = make_stringf("...were %s for their %s!",
                                   smite[1].c_str(), sin[1].c_str());
            break;
        case 16:
            turn[1] = make_stringf("Zin looked out upon the remains of the %s...",
                                   sinner.c_str());
            turn[2] = make_stringf("...and declared it good that they had been...");
            turn[3] = make_stringf("...%s. And thus justice was done.", smite[1].c_str());
            break;
        case 17:
            turn[1] = make_stringf("The law of Zin demands thee...");
            turn[2] = make_stringf("...be %s, and that the punishment for %s...",
                                   virtue[0].c_str(), sin[1].c_str());
            turn[3] = make_stringf("...shall be swift and harsh!");
            break;
        case 18:
            turn[1] = make_stringf("It was then that Zin bade them...");
            turn[2] = make_stringf("...not to stray from %s, lest...", virtue[1].c_str());
            turn[3] = make_stringf("...they become as damned as the %s.", sinner.c_str());
            break;
        case 19:
            turn[1] = make_stringf("Only the %s shall be judged worthy, and...",
                                   virtue[0].c_str());
            turn[2] = make_stringf("...all the %s will be found wanting. Such is...",
                                   sinner.c_str());
            turn[3] = make_stringf("...the word of Zin, and such is the law!");
            break;
        case 20:
            turn[1] = make_stringf("To those who would swear an oath of %s on my altar...",
                                   virtue[1].c_str());
            turn[2] = make_stringf("...I bring ye salvation. To the rest, ye %s...",
                                   sinner.c_str());
            turn[3] = make_stringf("...and the %s, the name of Zin shall be thy damnation.",
                                   sin[0].c_str());
            break;
        case 21:
            turn[1] = make_stringf("And Zin decreed that the people would be...");
            turn[2] = make_stringf("...protected from %s in all its forms, and...",
                                   sin[1].c_str());
            turn[3] = make_stringf("...preserved in their %s for all the days to come.",
                                   virtue[1].c_str());
            break;
        case 22:
            turn[1] = make_stringf("For those who would enter Zin's holy bosom...");
            turn[2] = make_stringf("...and live in %s, Zin provideth. Such is...",
                                   virtue[1].c_str());
            turn[3] = make_stringf("...the covenant, and such is the way of things.");
            break;
        case 23:
            turn[1] = make_stringf("Zin hath not damned the %s, but it is they...",
                                   sinner.c_str());
            turn[2] = make_stringf("...that have damned themselves for their %s, for...",
                                   sin[1].c_str());
            turn[3] = make_stringf("...did Zin not decree that to be %s was wrong?",
                                   sin[0].c_str());
            break;
        case 24:
            turn[1] = make_stringf("And Zin, furious at their %s, held...", sin[1].c_str());
            turn[2] = make_stringf("...aloft a silver sceptre! The %s...", sinner.c_str());
            turn[3] = make_stringf("...were %s, and thus the way of things was maintained.",
                                   smite[1].c_str());
            break;
        case 25:
            turn[1] = make_stringf("When the law of the land faltered, Zin rose...");
            turn[2] = make_stringf("...from the silver throne, and the %s were...",
                                   sinner.c_str());
            turn[3] = make_stringf("...%s. And it was thus that the law was made good.",
                                   smite[1].c_str());
            break;
        case 26:
            turn[1] = make_stringf("Zin descended from on high in a silver chariot...");
            turn[2] = make_stringf("...to %s the %s for their...",
                                   smite[0].c_str(), sinner.c_str());
            turn[3] = make_stringf("...%s, and thus judgement was rendered.",
                                   sin[1].c_str());
            break;
        case 27:
            turn[1] = make_stringf("The %s stood before Zin, and in that instant...",
                                   sinner.c_str());
            turn[2] = make_stringf("...they knew they would be found guilty of %s...",
                                   sin[1].c_str());
            turn[3] = make_stringf("...for that is the word of Zin, and Zin's word is law.");
            break;
    }

    string recite = "Hail Satan.";

    if (step == -1)
    {
        string bookname = (prayertype == RECITE_CHAOTIC)  ?  "Abominations"  :
                          (prayertype == RECITE_IMPURE)   ?  "Ablutions"     :
                          (prayertype == RECITE_HERETIC)  ?  "Apostates"     :
                          (prayertype == RECITE_UNHOLY)   ?  "Anathema"      :
                                                             "Bugginess";
        ostringstream numbers;
        numbers << chapter;
        numbers << ":";
        numbers << verse;
        recite = bookname + " " + numbers.str();
    }
    else
        recite = turn[step];

    return recite;
}

/** How vulnerable to RECITE_HERETIC is this monster?
 *
 * @param[in] mon The monster to check.
 * @returns the susceptibility.
 */
static int _heretic_recite_weakness(const monster *mon)
{
    int degree = 0;

    // Sleeping or paralyzed monsters will wake up or still perceive their
    // surroundings, respectively.  So, you can still recite to them.
    if (mons_intel(mon) >= I_NORMAL
        && !(mon->has_ench(ENCH_DUMB) || mons_is_confused(mon)))
    {
        // In the eyes of Zin, everyone is a sinner until proven otherwise!
            degree++;

        // Any priest is a heretic...
        if (mon->is_priest())
            degree++;

        // Or those who believe in themselves...
        if (mon->type == MONS_DEMIGOD)
            degree++;

        // ...but evil gods are worse.
        if (is_evil_god(mon->god) || is_unknown_god(mon->god))
            degree++;

        // (The above mean that worshipers will be treated as
        // priests for reciting, even if they aren't actually.)


        // Sanity check: monsters that won't attack you, and aren't
        // priests/evil, don't get recited against.
        if (mon->wont_attack() && degree <= 1)
            degree = 0;

        // Sanity check: monsters that are holy, know holy spells, or worship
        // holy gods aren't heretics.
        if (mon->is_holy() || is_good_god(mon->god))
            degree = 0;
    }

    return degree;
}

typedef FixedVector<int, NUM_RECITE_TYPES> recite_counts;
/** Check whether this monster might be influenced by Recite.
 *
 * @param[in] mon The monster to check.
 * @param[out] eligibility A vector, indexed by recite_type, that indicates
 *             which recitation types the monster is affected by, if any:
 *             eligibility[RECITE_FOO] is nonzero if the monster is affected
 *             by RECITE_FOO. Only modified if the function returns 0 or 1.
 * @param quiet[in]     Whether to suppress messenging.
 * @return  -1 if the monster is already affected. The eligibility vector
 *          is unchanged.
 * @return  0 if the monster is otherwise ineligible for recite. The
 *          eligibility vector is filled with zeros.
 * @return  1 if the monster is eligible for recite. The eligibility vector
 *          indicates which types of recitation it is vulnerable to.
 */
static int _zin_check_recite_to_single_monster(const monster *mon,
                                               recite_counts &eligibility,
                                               bool quiet = false)
{
    ASSERT(mon);

    // Can't recite if they were recently recited to.
    if (mon->has_ench(ENCH_RECITE_TIMER))
        return -1;

    const mon_holy_type holiness = mon->holiness();

    eligibility.init(0);

    // Anti-chaos prayer: Hits things vulnerable to silver, or with chaotic spells/gods.
    eligibility[RECITE_CHAOTIC] = mon->how_chaotic(true);

    // Anti-impure prayer: Hits things that Zin hates in general.
    // Don't look at the monster's god; that's what RECITE_HERETIC is for.
    eligibility[RECITE_IMPURE] = mon->how_unclean(false);
    // Sanity check: if a monster is 'really' natural, don't consider it impure.
    if (mons_intel(mon) < I_NORMAL
        && (holiness == MH_NATURAL || holiness == MH_PLANT)
        && mon->type != MONS_UGLY_THING
        && mon->type != MONS_VERY_UGLY_THING
        && mon->type != MONS_DEATH_DRAKE)
    {
        eligibility[RECITE_IMPURE] = 0;
    }

    // Anti-unholy prayer: Hits demons and incorporeal undead.
    if (holiness == MH_UNDEAD && mon->is_insubstantial()
        || holiness == MH_DEMONIC)
    {
        eligibility[RECITE_UNHOLY]++;
    }

    // Anti-heretic prayer: Hits intelligent monsters, especially priests.
    eligibility[RECITE_HERETIC] = _heretic_recite_weakness(mon);

#ifdef DEBUG_DIAGNOSTICS
    if (!quiet)
    {
        string elig;
        for (int i = 0; i < NUM_RECITE_TYPES; i++)
            elig += '0' + eligibility[i];
        dprf("Eligibility: %s", elig.c_str());
    }
#endif

    // Checking to see whether they were eligible for anything at all.
    for (int i = 0; i < NUM_RECITE_TYPES; i++)
        if (eligibility[i] > 0)
            return 1;

    return 0;
}

bool zin_check_able_to_recite(bool quiet)
{
    if (you.duration[DUR_RECITE])
    {
        if (!quiet)
            mpr("Finish your current sermon first, please.");
        return false;
    }

    if (you.duration[DUR_BREATH_WEAPON])
    {
        if (!quiet)
            mpr("You're too short of breath to recite.");
        return false;
    }

    if (you.duration[DUR_WATER_HOLD] && !you.res_water_drowning())
    {
        if (!quiet)
            mpr("You cannot recite while unable to breathe!");
        return false;
    }

    return true;
}

/**
 * Check whether there are monsters who might be influenced by Recite.
 * If prayertype is null, we're just checking whether we can.
 * Otherwise we're actually reciting, and may need to present a menu.
 *
 * @param quiet     Whether to suppress messages.
 * @return  0 if no monsters were found, or if the player declined to choose
 *          a type of recitation, or if all the found monsters returned
 *          zero from _zin_check_recite_to_single_monster().
 * @return  1 if an eligible audience was found.
 * @return  -1 if only an ineligible audience was found: no eligibile
 *          monsters, and at least one returned -1 from
 *          _zin_check_recite_to_single_monster().
 */
int zin_check_recite_to_monsters(bool quiet)
{
    bool found_ineligible = false;
    bool found_eligible = false;
    recite_counts count(0);

    map<string, int> affected_by_type[NUM_RECITE_TYPES];

    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        const monster *mon = monster_at(*ri);
        if (!mon || !you.can_see(mon))
            continue;

        recite_counts retval;
        switch (_zin_check_recite_to_single_monster(mon, retval, quiet))
        {
        case -1:
            found_ineligible = true;
        case 0:
            continue;
        }

        for (int i = 0; i < NUM_RECITE_TYPES; i++)
            if (retval[i] > 0)
            {
                count[i]++;
                found_eligible = true;
                ++affected_by_type[i][mon->full_name(DESC_A)];
            }
    }

    if (!found_eligible && !found_ineligible)
    {
        if (!quiet)
            dprf("No audience found!");
        return 0;
    }
    else if (!found_eligible && found_ineligible)
    {
        if (!quiet)
            dprf("No sensible audience found!");
        return -1;
    }

    int eligible_types = 0;
    for (int i = 0; i < NUM_RECITE_TYPES; i++)
        if (count[i] > 0)
            eligible_types++;

    return 1; // We just recite against everything.
}

enum zin_eff
{
    ZIN_NOTHING,
    ZIN_DAZE,
    ZIN_CONFUSE,
    ZIN_PARALYSE,
    ZIN_BLEED,
    ZIN_SMITE,
    ZIN_BLIND,
    ZIN_SILVER_CORONA,
    ZIN_ANTIMAGIC,
    ZIN_MUTE,
    ZIN_MAD,
    ZIN_DUMB,
    ZIN_IGNITE_CHAOS,
    ZIN_SALTIFY,
    ZIN_ROT,
    ZIN_HOLY_WORD,
};

bool zin_recite_to_single_monster(const coord_def& where)
{
    // That's a pretty good sanity check, I guess.
    ASSERT(you_worship(GOD_ZIN));

    monster* mon = monster_at(where);

    // Once you're already reciting, invis is ok.
    if (!mon || !cell_see_cell(where, you.pos(), LOS_DEFAULT))
        return false;

    recite_counts eligibility;
    bool affected = false;

    if (_zin_check_recite_to_single_monster(mon, eligibility) < 1)
        return false;

    recite_type prayertype = RECITE_HERETIC;
    for (int i = NUM_RECITE_TYPES - 1; i >= RECITE_HERETIC; i--)
    {
        if (eligibility[i] > 0)
        {
            prayertype = static_cast <recite_type>(i);
            break;
        }
    }

    // Second check: because this affects the whole screen over several turns,
    // its effects are staggered. There's a 50% chance per monster, per turn,
    // that nothing will happen - so the cumulative odds of nothing happening
    // are one in eight, since you recite three times.
    if (coinflip())
        return false;

    // Resistance is now based on HD.
    // You can affect up to (30+30)/2 = 30 'power' (HD).
    const int power_mult = 10;
    const int invo_power = you.skill_rdiv(SK_INVOCATIONS, power_mult)
                           + 3 * power_mult;
    const int piety_power = you.piety * 3 / 2;
    const int power = (invo_power + piety_power) / 2 / power_mult;
    // Old recite was mostly deterministic, which is bad.
    const int resist = mon->get_hit_dice() + random2(6);
    const int check = power - resist;

    // We abort if we didn't *beat* their HD - but first we might get a cute message.
    if (mon->can_speak() && one_chance_in(5))
    {
        if (check < -10)
            simple_monster_message(mon, " guffaws at your puny god.");
        else if (check < -5)
            simple_monster_message(mon, " sneers at your recitation.");
    }

    if (check <= 0)
        return false;

    // To what degree are they eligible for this prayertype?
    const int degree = eligibility[prayertype];
    const bool minor = degree <= (prayertype == RECITE_HERETIC ? 2 : 1);
    const int spellpower = power * 2 + degree * 20;
    zin_eff effect = ZIN_NOTHING;

    switch (prayertype)
    {
    case RECITE_HERETIC:
        if (degree == 1)
        {
            if (mon->asleep())
                break;
            // This is the path for 'conversion' effects.
            // Their degree is only 1 if they weren't a priest,
            // a worshiper of an evil or chaotic god, etc.

            // Right now, it only has the 'failed conversion' effects, though.
            // This branch can't hit sleeping monsters - until they wake up.

            if (check < 5)
                effect = ZIN_DAZE;
            else if (check < 10)
            {
                if (coinflip())
                    effect = ZIN_CONFUSE;
                else
                    effect = ZIN_DAZE;
            }
            else if (check < 15)
                effect = ZIN_CONFUSE;
            else
            {
                if (one_chance_in(3))
                    effect = ZIN_PARALYSE;
                else
                    effect = ZIN_CONFUSE;
            }
        }
        else
        {
            // This is the path for 'smiting' effects.
            // Their degree is only greater than 1 if
            // they're unable to be redeemed.
            if (check < 5)
            {
                if (coinflip())
                    effect = ZIN_BLEED;
                else
                    effect = ZIN_SMITE;
            }
            else if (check < 10)
            {
                if (one_chance_in(3))
                    effect = ZIN_BLIND;
                else if (mon->antimagic_susceptible())
                    effect = ZIN_ANTIMAGIC;
                else
                    effect = ZIN_SILVER_CORONA;
            }
            else if (check < 15)
            {
                if (one_chance_in(3))
                    effect = ZIN_BLIND;
                else if (coinflip())
                    effect = ZIN_PARALYSE;
                else
                    effect = ZIN_MUTE;
            }
            else
            {
                if (coinflip())
                    effect = ZIN_MAD;
                else
                    effect = ZIN_DUMB;
            }
        }
        break;

    case RECITE_CHAOTIC:
        if (check < 5)
        {
            // nastier -- fallthrough if immune
            if (coinflip() && mon->can_bleed())
                effect = ZIN_BLEED;
            else
                effect = ZIN_SMITE;
        }
        else if (check < 10)
        {
            if (coinflip())
                effect = ZIN_SILVER_CORONA;
            else
                effect = ZIN_SMITE;
        }
        else if (check < 15)
        {
            if (coinflip())
                effect = ZIN_IGNITE_CHAOS;
            else
                effect = ZIN_SILVER_CORONA;
        }
        else
            effect = ZIN_SALTIFY;
        break;

    case RECITE_IMPURE:
        // Many creatures normally resistant to rotting are still affected,
        // because this is divine punishment.  Those with no real flesh are
        // immune, of course.
        if (check < 5)
        {
            if (coinflip() && mon->can_bleed())
                effect = ZIN_BLEED;
            else
                effect = ZIN_SMITE;
        }
        else if (check < 10)
        {
            if (coinflip() && mon->res_rotting() <= 1)
                effect = ZIN_ROT;
            else
                effect = ZIN_SILVER_CORONA;
        }
        else if (check < 15)
        {
            if (mon->undead_or_demonic() && coinflip())
                effect = ZIN_HOLY_WORD;
            else
                effect = ZIN_SILVER_CORONA;
        }
        else
            effect = ZIN_SALTIFY;
        break;

    case RECITE_UNHOLY:
        if (check < 5)
        {
            if (mons_intel(mon) > I_INSECT && coinflip())
                effect = ZIN_DAZE;
            else
                effect = ZIN_CONFUSE;
        }
        else if (check < 10)
        {
            if (coinflip())
                effect = ZIN_CONFUSE;
            else
                effect = ZIN_SILVER_CORONA;
        }
        // Half of the time, the anti-unholy prayer will be capped at this
        // level of effect.
        else if (check < 15 || coinflip())
        {
            if (coinflip())
                effect = ZIN_HOLY_WORD;
            else
                effect = ZIN_SILVER_CORONA;
        }
        else
            effect = ZIN_SALTIFY;
        break;

    case NUM_RECITE_TYPES:
        die("invalid recite type");
    }

    // And the actual effects...
    switch (effect)
    {
    case ZIN_NOTHING:
        break;

    case ZIN_DAZE:
        if (mon->add_ench(mon_enchant(ENCH_DAZED, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(mon, " is dazed by your recitation.");
            affected = true;
        }
        break;

    case ZIN_CONFUSE:
        if (mons_class_is_confusable(mon->type)
            && !mon->check_clarity(false)
            && mon->add_ench(mon_enchant(ENCH_CONFUSION, degree, &you,
                             (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            if (prayertype == RECITE_HERETIC)
                simple_monster_message(mon, " is confused by your recitation.");
            else
                simple_monster_message(mon, " stumbles about in disarray.");
            affected = true;
        }
        break;

    case ZIN_PARALYSE:
        if (mon->add_ench(mon_enchant(ENCH_PARALYSIS, 0, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(mon,
                minor ? " is awed by your recitation."
                      : " is aghast at the heresy of your recitation.");
            affected = true;
        }
        break;

    case ZIN_BLEED:
        if (mon->can_bleed()
            && mon->add_ench(mon_enchant(ENCH_BLEED, degree, &you,
                             (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            mon->add_ench(mon_enchant(ENCH_SICK, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY));
            switch (prayertype)
            {
            case RECITE_HERETIC:
                if (minor)
                    simple_monster_message(mon, "'s eyes and ears begin to bleed.");
                else
                {
                    mprf("%s bleeds profusely from %s eyes and ears.",
                         mon->name(DESC_THE).c_str(),
                         mon->pronoun(PRONOUN_POSSESSIVE).c_str());
                }
                break;
            case RECITE_CHAOTIC:
                simple_monster_message(mon,
                    minor ? "'s chaotic flesh is covered in bleeding sores."
                          : "'s chaotic flesh erupts into weeping sores!");
                break;
            case RECITE_IMPURE:
                simple_monster_message(mon,
                    minor ? "'s impure flesh is covered in bleeding sores."
                          : "'s impure flesh erupts into weeping sores!");
                break;
            default:
                die("bad recite bleed");
            }
            affected = true;
        }
        break;

    case ZIN_SMITE:
        if (minor)
            simple_monster_message(mon, " is smitten by the wrath of Zin.");
        else
            simple_monster_message(mon, " is blasted by the fury of Zin!");
        // XXX: This duplicates code in cast_smiting().
        mon->hurt(&you, 7 + (random2(spellpower) * 33 / 191));
        if (mon->alive())
            print_wounds(mon);
        affected = true;
        break;

    case ZIN_BLIND:
        if (mon->add_ench(mon_enchant(ENCH_BLIND, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(mon, " is struck blind by the wrath of Zin!");
            affected = true;
        }
        break;

    case ZIN_SILVER_CORONA:
        if (mon->add_ench(mon_enchant(ENCH_SILVER_CORONA, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(mon, " is limned with silver light.");
            affected = true;
        }
        break;

    case ZIN_ANTIMAGIC:
        ASSERT(prayertype == RECITE_HERETIC);
        if (mon->add_ench(mon_enchant(ENCH_ANTIMAGIC, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(mon,
                minor ? " quails at your recitation."
                      : " looks feeble and powerless before your recitation.");
            affected = true;
        }
        break;

    case ZIN_MUTE:
        if (mon->add_ench(mon_enchant(ENCH_MUTE, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(mon, " is struck mute by the wrath of Zin!");
            affected = true;
        }
        break;

    case ZIN_MAD:
        if (mon->add_ench(mon_enchant(ENCH_MAD, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(mon, " is driven mad by the wrath of Zin!");
            affected = true;
        }
        break;

    case ZIN_DUMB:
        if (mon->add_ench(mon_enchant(ENCH_DUMB, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(mon, " is left stupefied by the wrath of Zin!");
            affected = true;
        }
        break;

    case ZIN_IGNITE_CHAOS:
        ASSERT(prayertype == RECITE_CHAOTIC);
        {
            bolt beam;
            dice_def dam_dice(0, 5 + spellpower/7);  // Dice added below if applicable.
            dam_dice.num = degree;

            int damage = dam_dice.roll();
            if (damage > 0)
            {
                mon->hurt(&you, damage, BEAM_MISSILE, false);

                if (mon->alive())
                {
                    simple_monster_message(mon,
                      (damage < 25) ? "'s chaotic flesh sizzles and spatters!" :
                      (damage < 50) ? "'s chaotic flesh bubbles and boils."
                                    : "'s chaotic flesh runs like molten wax.");

                    print_wounds(mon);
                    behaviour_event(mon, ME_WHACK, &you);
                    affected = true;
                }
                else
                {
                    simple_monster_message(mon,
                        " melts away into a sizzling puddle of chaotic flesh.");
                    monster_die(mon, KILL_YOU, NON_MONSTER);
                }
            }
        }
        break;

    case ZIN_SALTIFY:
        _zin_saltify(mon);
        break;

    case ZIN_ROT:
        ASSERT(prayertype == RECITE_IMPURE);
        if (mon->res_rotting() <= 1
            && mon->add_ench(mon_enchant(ENCH_ROT, degree, &you,
                             (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            mon->add_ench(mon_enchant(ENCH_SICK, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY));
            simple_monster_message(mon,
                minor ? "'s impure flesh begins to rot away."
                      : "'s impure flesh sloughs off!");
            affected = true;
        }
        break;

    case ZIN_HOLY_WORD:
        holy_word_monsters(where, spellpower, HOLY_WORD_ZIN, &you);
        affected = true;
        break;
    }

    // Recite time, to prevent monsters from being recited against
    // more than once in a given recite instance.
    if (affected)
        mon->add_ench(mon_enchant(ENCH_RECITE_TIMER, degree, &you, 40));

    // Monsters that have been affected may shout.
    if (affected
        && one_chance_in(3)
        && mon->alive()
        && !mon->asleep()
        && !mon->cannot_move()
        && mons_shouts(mon->type, false) != S_SILENT)
    {
        handle_monster_shouts(mon, true);
    }

    return true;
}

static void _zin_saltify(monster* mon)
{
    const coord_def where = mon->pos();
    const monster_type pillar_type =
        mons_is_zombified(mon) ? mons_zombie_base(mon)
                               : mons_species(mon->type);
    const int hd = mon->get_hit_dice();

    simple_monster_message(mon, " is turned into a pillar of salt by the wrath of Zin!");

    // If the monster leaves a corpse when it dies, destroy the corpse.
    int corpse = monster_die(mon, KILL_YOU, NON_MONSTER);
    if (corpse != -1)
        destroy_item(corpse);

    if (monster *pillar = create_monster(
                        mgen_data(MONS_PILLAR_OF_SALT,
                                  BEH_HOSTILE,
                                  0,
                                  0,
                                  0,
                                  where,
                                  MHITNOT,
                                  MG_FORCE_PLACE,
                                  GOD_NO_GOD,
                                  pillar_type),
                                  false))
    {
        // Enemies with more HD leave longer-lasting pillars of salt.
        int time_left = (random2(8) + hd) * BASELINE_DELAY;
        mon_enchant temp_en(ENCH_SLOWLY_DYING, 1, 0, time_left);
        pillar->update_ench(temp_en);
    }
}

void zin_recite_interrupt()
{
    if (!you.duration[DUR_RECITE])
        return;
    mprf(MSGCH_DURATION, "Your recitation is interrupted.");
    mpr("You feel short of breath.");
    you.duration[DUR_RECITE] = 0;

    you.increase_duration(DUR_BREATH_WEAPON, random2(10) + random2(30));
}

bool zin_vitalisation()
{
    simple_god_message(" grants you divine stamina.");

    // Feed the player slightly.
    if (you.hunger_state < HS_FULL)
        lessen_hunger(250, false);

    // Add divine stamina.
    const int stamina_amt = max(1, you.skill_rdiv(SK_INVOCATIONS, 1, 3));
    you.attribute[ATTR_DIVINE_STAMINA] = stamina_amt;
    you.set_duration(DUR_DIVINE_STAMINA, 60 + roll_dice(2, 10));

    notify_stat_change(STAT_STR, stamina_amt, true);
    notify_stat_change(STAT_INT, stamina_amt, true);
    notify_stat_change(STAT_DEX, stamina_amt, true);

    return true;
}

void zin_remove_divine_stamina()
{
    mprf(MSGCH_DURATION, "Your divine stamina fades away.");
    notify_stat_change(STAT_STR, -you.attribute[ATTR_DIVINE_STAMINA], true);
    notify_stat_change(STAT_INT, -you.attribute[ATTR_DIVINE_STAMINA], true);
    notify_stat_change(STAT_DEX, -you.attribute[ATTR_DIVINE_STAMINA], true);
    you.duration[DUR_DIVINE_STAMINA] = 0;
    you.attribute[ATTR_DIVINE_STAMINA] = 0;
}

bool zin_remove_all_mutations()
{
    if (!how_mutated())
    {
        mpr("You have no mutations to be cured!");
        return false;
    }

    you.one_time_ability_used.set(GOD_ZIN);
    take_note(Note(NOTE_GOD_GIFT, you.religion));

    simple_god_message(" draws all chaos from your body!");
    delete_all_mutations("Zin's power");

    return true;
}

bool zin_sanctuary()
{
    // Casting is disallowed while previous sanctuary in effect.
    // (Checked in ability.cc.)
    if (env.sanctuary_time)
        return false;

    // Yes, shamelessly stolen from NetHack...
    if (!silenced(you.pos())) // How did you manage that?
        mprf(MSGCH_SOUND, "You hear a choir sing!");
    else
        mpr("You are suddenly bathed in radiance!");

    flash_view(UA_PLAYER, WHITE);

    holy_word(100, HOLY_WORD_ZIN, you.pos(), true, &you);

#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    scaled_delay(1000);
#endif

    // Pets stop attacking and converge on you.
    you.pet_target = MHITYOU;

    create_sanctuary(you.pos(), 7 + you.skill_rdiv(SK_INVOCATIONS) / 2);

    return true;
}

// shield bonus = attribute for duration turns, then decreasing by 1
//                every two out of three turns
// overall shield duration = duration + attribute
// recasting simply resets those two values (to better values, presumably)
void tso_divine_shield()
{
    if (!you.duration[DUR_DIVINE_SHIELD])
    {
        if (you.shield()
            || you.duration[DUR_CONDENSATION_SHIELD])
        {
            mprf("Your shield is strengthened by %s divine power.",
                 apostrophise(god_name(GOD_SHINING_ONE)).c_str());
        }
        else
            mpr("A divine shield forms around you!");
    }
    else
        mpr("Your divine shield is renewed.");

    you.redraw_armour_class = true;

    // duration of complete shield bonus from 35 to 80 turns
    you.set_duration(DUR_DIVINE_SHIELD,
                     35 + you.skill_rdiv(SK_INVOCATIONS, 4, 3));

    // affects size of SH bonus, decreases near end of duration
    you.attribute[ATTR_DIVINE_SHIELD] = 3 + you.skill_rdiv(SK_INVOCATIONS, 1, 5);

    you.redraw_armour_class = true;
}

void tso_remove_divine_shield()
{
    mprf(MSGCH_DURATION, "Your divine shield disappears!");
    you.duration[DUR_DIVINE_SHIELD] = 0;
    you.attribute[ATTR_DIVINE_SHIELD] = 0;
    you.redraw_armour_class = true;
}

void elyvilon_purification()
{
    mpr("You feel purified!");

    you.disease = 0;
    you.rotting = 0;
    you.duration[DUR_POISONING] = 0;
    you.duration[DUR_CONF] = 0;
    you.duration[DUR_SLOW] = 0;
    you.duration[DUR_PETRIFYING] = 0;
    you.duration[DUR_WEAK] = 0;
    restore_stat(STAT_ALL, 0, false);
    unrot_hp(9999);
    you.redraw_evasion = true;
}

bool elyvilon_divine_vigour()
{
    bool success = false;

    if (!you.duration[DUR_DIVINE_VIGOUR])
    {
        mprf("%s grants you divine vigour.",
             god_name(GOD_ELYVILON).c_str());

        const int vigour_amt = 1 + you.skill_rdiv(SK_INVOCATIONS, 1, 3);
        const int old_hp_max = you.hp_max;
        const int old_mp_max = you.max_magic_points;
        you.attribute[ATTR_DIVINE_VIGOUR] = vigour_amt;
        you.set_duration(DUR_DIVINE_VIGOUR,
                         40 + you.skill_rdiv(SK_INVOCATIONS, 5, 2));

        calc_hp();
        inc_hp((you.hp_max * you.hp + old_hp_max - 1)/old_hp_max - you.hp);
        calc_mp();
        if (old_mp_max > 0)
        {
            inc_mp((you.max_magic_points * you.magic_points + old_mp_max - 1)
                     / old_mp_max
                   - you.magic_points);
        }

        success = true;
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return success;
}

void elyvilon_remove_divine_vigour()
{
    mprf(MSGCH_DURATION, "Your divine vigour fades away.");
    you.duration[DUR_DIVINE_VIGOUR] = 0;
    you.attribute[ATTR_DIVINE_VIGOUR] = 0;
    calc_hp();
    calc_mp();
}

bool vehumet_supports_spell(spell_type spell)
{
    if (spell_typematch(spell, SPTYP_CONJURATION))
        return true;

    // Conjurations work by conjuring up a chunk of short-lived matter and
    // propelling it towards the victim.  This is the most popular way, but
    // by no means it has a monopoly for being destructive.
    // Vehumet loves all direct physical destruction.
    if (spell == SPELL_SHATTER
        || spell == SPELL_LRD
        || spell == SPELL_SANDBLAST
        || spell == SPELL_AIRSTRIKE
        || spell == SPELL_TORNADO
        || spell == SPELL_FREEZE
        || spell == SPELL_IGNITE_POISON
        || spell == SPELL_OZOCUBUS_REFRIGERATION
        || spell == SPELL_OLGREBS_TOXIC_RADIANCE
        || spell == SPELL_INNER_FLAME)
    {
        return true;
    }

    return false;
}

// Returns false if the invocation fails (no spellbooks in sight, etc.).
bool trog_burn_spellbooks()
{
    if (!you_worship(GOD_TROG))
        return false;

    god_acting gdact;

    // XXX: maybe this should be allowed with less than immunity.
    if (player_res_fire(false) <= 3)
    {
        for (stack_iterator si(you.pos()); si; ++si)
        {
            if (item_is_spellbook(*si))
            {
                mprf("Burning your own %s might not be such a smart idea!",
                        you.foot_name(true).c_str());
                return false;
            }
        }
    }

    int totalpiety = 0;
    int totalblocked = 0;
    vector<coord_def> mimics;

    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        const unsigned short cloud = env.cgrid(*ri);
        int count = 0;
        int rarity = 0;
        for (stack_iterator si(*ri); si; ++si)
        {
            if (!item_is_spellbook(*si))
                continue;

            // If a grid is blocked, books lying there will be ignored.
            // Allow bombing of monsters.
            if (cell_is_solid(*ri)
                || cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE)
            {
                totalblocked++;
                continue;
            }

            if (si->flags & ISFLAG_MIMIC)
            {
                totalblocked++;
                mimics.push_back(*ri);
                continue;
            }

            // Ignore {!D} inscribed books.
            if (!check_warning_inscriptions(*si, OPER_DESTROY))
            {
                mpr("Won't ignite {!D} inscribed spellbook.");
                continue;
            }

            totalpiety += 2;

            // Rarity influences the duration of the pyre.
            rarity += book_rarity(si->sub_type);

            dprf("Burned spellbook rarity: %d", rarity);
            destroy_spellbook(*si);
            item_was_destroyed(*si);
            destroy_item(si.link());
            count++;
        }

        if (count)
        {
            if (cloud != EMPTY_CLOUD)
            {
                // Reinforce the cloud.
                mpr("The fire roars with new energy!");
                const int extra_dur = count + random2(rarity / 2);
                env.cloud[cloud].decay += extra_dur * 5;
                env.cloud[cloud].set_whose(KC_YOU);
                continue;
            }

            const int duration = min(4 + count + random2(rarity/2), 23);
            place_cloud(CLOUD_FIRE, *ri, duration, &you);

            mprf(MSGCH_GOD, "The spellbook%s burst%s into flames.",
                 count == 1 ? ""  : "s",
                 count == 1 ? "s" : "");
        }
    }

    if (totalpiety)
    {
        simple_god_message(" is delighted!", GOD_TROG);
        gain_piety(totalpiety);
    }
    else if (totalblocked)
    {
        mprf("The spellbook%s fail%s to ignite!",
             totalblocked == 1 ? ""  : "s",
             totalblocked == 1 ? "s" : "");
        for (auto c : mimics)
            discover_mimic(c);
        return false;
    }
    else
    {
        mpr("You cannot see a spellbook to ignite!");
        return false;
    }

    return true;
}

void trog_do_trogs_hand(int pow)
{
    you.increase_duration(DUR_TROGS_HAND,
                          5 + roll_dice(2, pow / 3 + 1), 100,
                          "Your skin crawls.");
    mprf(MSGCH_DURATION, "You feel resistant to hostile enchantments.");
}

void trog_remove_trogs_hand()
{
    if (you.duration[DUR_REGENERATION] == 0)
        mprf(MSGCH_DURATION, "Your skin stops crawling.");
    mprf(MSGCH_DURATION, "You feel less resistant to hostile enchantments.");
    you.duration[DUR_TROGS_HAND] = 0;
}

bool beogh_water_walk()
{
    return in_good_standing(GOD_BEOGH, 4);
}

/**
 * Has the monster been given a Beogh gift?
 *
 * @param mon the orc in question.
 * @returns whether you have given the monster a Beogh gift before now.
 */
static bool _given_gift(const monster* mon)
{
    return mon->props.exists(BEOGH_WPN_GIFT_KEY)
            || mon->props.exists(BEOGH_ARM_GIFT_KEY)
            || mon->props.exists(BEOGH_SH_GIFT_KEY);
}

/**
 * Checks whether the target monster is a valid target for beogh item-gifts.
 *
 * @param mons[in]  The monster to consider giving an item to.
 * @param quiet     Whether to print messages if the target is invalid.
 * @return          Whether the player can give an item to the monster.
 */
bool beogh_can_gift_items_to(const monster* mons, bool quiet)
{
    if (!mons || !mons->visible_to(&you))
    {
        if (!quiet)
            canned_msg(MSG_NOTHING_THERE);
        return false;
    }

    if (!is_orcish_follower(mons))
    {
        if (!quiet)
            mpr("That's not an orcish ally!");
        return false;
    }

    if (!mons->is_named())
    {
        if (!quiet)
            mpr("That orc has not proved itself worthy of your gift.");
        return false;
    }

    if (_given_gift(mons))
    {
        if (!quiet)
        {
            mprf("%s has already been given a gift.",
                 mons->name(DESC_THE, false).c_str());
        }
        return false;
    }

    return true;
}

/**
 * Checks whether there are any valid targets for beogh gifts in LOS.
 */
static bool _valid_beogh_gift_targets_in_sight()
{
    for (monster_near_iterator rad(you.pos(), LOS_NO_TRANS); rad; ++rad)
        if (beogh_can_gift_items_to(*rad))
            return true;
    return false;
}

/**
 * Allow the player to give an item to a named orcish ally that hasn't
 * been given a gift before
 *
 * @returns whether an item was given.
 */
bool beogh_gift_item()
{
    if (!_valid_beogh_gift_targets_in_sight())
    {
        mpr("No worthy followers in sight.");
        return false;
    }

    dist spd;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_BEOGH_GIFTABLE;
    args.range = LOS_RADIUS;
    args.needs_path = false;
    args.may_target_monster = true;
    args.cancel_at_self = true;
    args.show_floor_desc = true;
    args.top_prompt = "Select a follower to give a gift to.";

    direction(spd, args);

    if (!spd.isValid)
        return false;

    monster* mons = monster_at(spd.target);
    if (!beogh_can_gift_items_to(mons, false))
        return false;

    int item_slot = prompt_invent_item("Give which item?",
                                       MT_INVLIST, OSEL_ANY, true);

    if (item_slot == PROMPT_ABORT || item_slot == PROMPT_NOTHING)
    {
        canned_msg(MSG_OK);
        return false;
    }

    item_def& gift = you.inv[item_slot];

    const bool shield = is_shield(gift);
    const bool body_armour = gift.base_type == OBJ_ARMOUR
                             && get_armour_slot(gift) == EQ_BODY_ARMOUR;
    const bool weapon = gift.base_type == OBJ_WEAPONS;
    const bool range_weapon = weapon && is_range_weapon(gift);
    const item_def* mons_weapon = mons->weapon();

    if (!(weapon && mons->could_wield(gift)
          || body_armour && check_armour_size(gift, mons->body_size())
          || shield
             && (!mons_weapon
                 || mons->hands_reqd(*mons_weapon) != HANDS_TWO)))
    {
        mprf("%s can't use that.", mons->name(DESC_THE, false).c_str());

        return false;
    }

    // if we're giving a ranged weapon to an orc holding a melee weapon in
    // their hands, or vice versa, put it in their carried slot instead.
    // this will of course drop anything that's there.
    const bool use_alt_slot = weapon && mons_weapon
                              && is_range_weapon(gift) !=
                                 is_range_weapon(*mons_weapon);

    mons->take_item(item_slot, body_armour ? MSLOT_ARMOUR :
                                    shield ? MSLOT_SHIELD :
                              use_alt_slot ? MSLOT_ALT_WEAPON :
                                             MSLOT_WEAPON);
    if (use_alt_slot)
        mons->swap_weapons(true);

    dprf("is_ranged weap: %d", range_weapon);
    if (range_weapon)
        gift_ammo_to_orc(mons, true); // give a small initial ammo freebie


    if (shield)
        mons->props[BEOGH_SH_GIFT_KEY] = true;
    else if (body_armour)
        mons->props[BEOGH_ARM_GIFT_KEY] = true;
    else
        mons->props[BEOGH_WPN_GIFT_KEY] = true;

    return true;
}

void jiyva_paralyse_jellies()
{
    mprf("You call upon nearby slimes to pray to %s.",
         god_name(you.religion).c_str());

    int jelly_count = 0;
    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        monster* mon = monster_at(*ri);
        const int dur = 16 + random2(9);
        if (mon != NULL && mons_is_slime(mon) && !mon->is_shapeshifter())
        {
            mon->add_ench(mon_enchant(ENCH_PARALYSIS, 0,
                                      &you, dur * BASELINE_DELAY));
            jelly_count++;
        }
    }

    if (jelly_count > 0)
    {
        if (jelly_count > 1)
            mpr("The nearby slimes join the prayer.");
        else
            mpr("A nearby slime joins the prayer.");

        lose_piety(max(5, min(jelly_count, 20)));
    }
}

bool jiyva_remove_bad_mutation()
{
    if (!how_mutated())
    {
        mpr("You have no bad mutations to be cured!");
        return false;
    }

    // Ensure that only bad mutations are removed.
    if (!delete_mutation(RANDOM_BAD_MUTATION, "Jiyva's power", true, false, true, true))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return false;
    }

    mpr("You feel cleansed.");
    return true;
}

bool yred_injury_mirror()
{
    return in_good_standing(GOD_YREDELEMNUL, 1)
           && you.duration[DUR_MIRROR_DAMAGE]
           && crawl_state.which_god_acting() != GOD_YREDELEMNUL;
}

bool yred_can_animate_dead()
{
    return in_good_standing(GOD_YREDELEMNUL, 2);
}

/**
 * Animates corpses/skeletons where you are or in LOS, depending on piety.
 *
 * @returns false if it did nothing and you could have known so.
 */
bool yred_animate_remains_or_dead()
{
    if (yred_can_animate_dead())
    {
        canned_msg(MSG_CALL_DEAD);

        animate_dead(&you, you.skill_rdiv(SK_INVOCATIONS) + 1, BEH_FRIENDLY,
                     MHITYOU, &you, "", GOD_YREDELEMNUL);
    }
    else
    {
        canned_msg(MSG_ANIMATE_REMAINS);

        if (animate_remains(you.pos(), CORPSE_BODY, BEH_FRIENDLY,
                            MHITYOU, &you, "", GOD_YREDELEMNUL) < 0)
        {
            mpr("There are no remains here to animate!");
            return false;
        }
    }
    return true;
}

void yred_make_enslaved_soul(monster* mon, bool force_hostile)
{
    ASSERT(mons_enslaved_body_and_soul(mon));

    add_daction(DACT_OLD_ENSLAVED_SOULS_POOF);
    remove_enslaved_soul_companion();

    const string whose = you.can_see(mon) ? apostrophise(mon->name(DESC_THE))
                                          : mon->pronoun(PRONOUN_POSSESSIVE);

    // Remove the monster's soul-enslaving enchantment, as it's no
    // longer needed.
    mon->del_ench(ENCH_SOUL_RIPE, false, false);

    // Remove the monster's invisibility enchantment. If we don't do
    // this, it'll stay invisible after being remade as a spectral thing
    // below.
    mon->del_ench(ENCH_INVIS, false, false);

    // If the monster's held in a net, get it out.
    mons_clear_trapping_net(mon);

    // Drop the monster's holy equipment, and keep wielding the rest.  Also
    // remove any of its active avatars.
    monster_drop_things(mon, false, is_holy_item);
    mon->remove_avatars();

    const monster orig = *mon;

    // Use the original monster type as the zombified type here, to get
    // the proper stats from it.
    define_zombie(mon, mon->type, MONS_SPECTRAL_THING);

    // If the original monster has been levelled up, its HD might be different
    // from its class HD, in which case its HP should be rerolled to match.
    if (mon->get_experience_level() != orig.get_experience_level())
    {
        mon->set_hit_dice(max(orig.get_experience_level(), 1));
        roll_zombie_hp(mon);
    }

    mon->colour = ETC_UNHOLY;

    mon->flags |= MF_NO_REWARD;
    mon->flags |= MF_ENSLAVED_SOUL;

    // If the original monster type has melee abilities, make sure
    // its spectral thing has them as well.
    mon->flags |= orig.flags & MF_MELEE_MASK;
    mon->spells = orig.spells;

    name_zombie(mon, &orig);

    mons_make_god_gift(mon, GOD_YREDELEMNUL);
    add_companion(mon);

    mon->attitude = !force_hostile ? ATT_FRIENDLY : ATT_HOSTILE;
    behaviour_event(mon, ME_ALERT, force_hostile ? &you : 0);

    mon->stop_constricting_all(false);
    mon->stop_being_constricted();

    mprf("%s soul %s.", whose.c_str(),
         !force_hostile ? "is now yours" : "fights you");
}

bool kiku_receive_corpses(int pow)
{
    // pow = necromancy * 4, ranges from 0 to 108
    dprf("kiku_receive_corpses() power: %d", pow);

    // Kiku gives branch-appropriate corpses (like shadow creatures).
    // 1d2 at 0 Nec, up to 8 at 27 Nec.
    int expected_extra_corpses = 1 + random2(2) + random2(pow / 18);
    int corpse_delivery_radius = 1;

    // We should get the same number of corpses
    // in a hallway as in an open room.
    int spaces_for_corpses = 0;
    for (radius_iterator ri(you.pos(), corpse_delivery_radius, C_ROUND,
                            LOS_NO_TRANS, true); ri; ++ri)
    {
        if (mons_class_can_pass(MONS_HUMAN, grd(*ri)))
            spaces_for_corpses++;
    }
    // floating over lava, heavy tomb abuse, etc
    if (!spaces_for_corpses)
        spaces_for_corpses++;

    int percent_chance_a_square_receives_extra_corpse = // can be > 100
        int(float(expected_extra_corpses) / float(spaces_for_corpses) * 100.0);

    int corpses_created = 0;

    for (radius_iterator ri(you.pos(), corpse_delivery_radius, C_ROUND,
                            LOS_NO_TRANS); ri; ++ri)
    {
        bool square_is_walkable = mons_class_can_pass(MONS_HUMAN, grd(*ri));
        bool square_is_player_square = (*ri == you.pos());
        bool square_gets_corpse =
            random2(100) < percent_chance_a_square_receives_extra_corpse
            || square_is_player_square && random2(100) < 97;

        if (!square_is_walkable || !square_gets_corpse)
            continue;

        corpses_created++;

        // Find an appropriate monster corpse for level and power.
        monster_type mon_type = MONS_PROGRAM_BUG;
        int adjusted_power = 0;
        for (int i = 0; i < 200 && !mons_class_can_be_zombified(mon_type); ++i)
        {
            adjusted_power = min(pow / 4, random2(random2(pow)));
            // Pick a place based on the power.  This may be below the branch's
            // start, that's ok.
            level_id lev(you.where_are_you, adjusted_power
                - absdungeon_depth(you.where_are_you, 0));
            mon_type = pick_local_zombifiable_monster(lev);
        }

        // Create corpse object.
        monster dummy;
        dummy.type = mon_type;
        define_monster(&dummy);
        int index_of_corpse_created = get_mitm_slot();

        if (index_of_corpse_created == NON_ITEM)
            break;

        int valid_corpse = fill_out_corpse(&dummy,
                                           dummy.type,
                                           mitm[index_of_corpse_created],
                                           false);
        if (valid_corpse == -1)
        {
            mitm[index_of_corpse_created].clear();
            continue;
        }

        mitm[index_of_corpse_created].props[NEVER_HIDE_KEY] = true;

        ASSERT(valid_corpse >= 0);

        // Higher piety means fresher corpses.
        int rottedness = 200 -
            (!one_chance_in(10) ? random2(200 - you.piety)
                                : random2(100 + random2(75)));
        rottedness = rottedness / 2 + 1; // hack to adjust for rotten corpse
                                         // removal
        mitm[index_of_corpse_created].special = rottedness;

        // Place the corpse.
        move_item_to_grid(&index_of_corpse_created, *ri);
    }

    if (corpses_created)
    {
        if (you_worship(GOD_KIKUBAAQUDGHA))
        {
            simple_god_message(corpses_created > 1 ? " delivers you corpses!"
                                                   : " delivers you a corpse!");
        }
        maybe_update_stashes();
        return true;
    }
    else
    {
        if (you_worship(GOD_KIKUBAAQUDGHA))
            simple_god_message(" can find no cadavers for you!");
        return false;
    }
}

/**
 * Destroy a corpse at the player's location
 *
 * @return  True if a corpse was destroyed, false otherwise.
*/
bool kiku_take_corpse()
{
    for (int i = you.visible_igrd(you.pos()); i != NON_ITEM; i = mitm[i].link)
    {
        item_def &item(mitm[i]);

        if (item.base_type != OBJ_CORPSES || item.sub_type != CORPSE_BODY)
            continue;
        item_was_destroyed(item);
        destroy_item(i);
        return true;
    }

    return false;
}

bool fedhas_passthrough_class(const monster_type mc)
{
    return you_worship(GOD_FEDHAS)
           && mons_class_is_plant(mc)
           && mons_class_is_stationary(mc)
           && mc != MONS_SNAPLASHER_VINE
           && mc != MONS_SNAPLASHER_VINE_SEGMENT;
}

// Fedhas allows worshipers to walk on top of stationary plants and
// fungi.
bool fedhas_passthrough(const monster* target)
{
    return target
           && fedhas_passthrough_class(target->type)
           && (mons_species(target->type) != MONS_OKLOB_PLANT
               || target->attitude != ATT_HOSTILE);
}

bool fedhas_passthrough(const monster_info* target)
{
    return target
           && fedhas_passthrough_class(target->type)
           && (mons_species(target->type) != MONS_OKLOB_PLANT
               || target->attitude != ATT_HOSTILE);
}

// Fedhas worshipers can shoot through non-hostile plants, can a
// particular beam go through a particular monster?
bool fedhas_shoot_through(const bolt& beam, const monster* victim)
{
    actor *originator = beam.agent();
    if (!victim || !originator)
        return false;

    bool origin_worships_fedhas;
    mon_attitude_type origin_attitude;
    if (originator->is_player())
    {
        origin_worships_fedhas = you_worship(GOD_FEDHAS);
        origin_attitude = ATT_FRIENDLY;
    }
    else
    {
        monster* temp = originator->as_monster();
        if (!temp)
            return false;
        origin_worships_fedhas = temp->god == GOD_FEDHAS;
        origin_attitude = temp->attitude;
    }

    return origin_worships_fedhas
           && fedhas_protects(victim)
           && !beam.is_enchantment()
           && !(beam.is_explosion && beam.in_explosion_phase)
           && beam.name != "lightning arc"
           && (mons_atts_aligned(victim->attitude, origin_attitude)
               || victim->neutral());
}

// Turns corpses in LOS into skeletons and grows toadstools on them.
// Can also turn zombies into skeletons and destroy ghoul-type monsters.
// Returns the number of corpses consumed.
int fedhas_fungal_bloom()
{
    int seen_mushrooms = 0;
    int seen_corpses = 0;
    int processed_count = 0;
    bool kills = false;

    for (radius_iterator i(you.pos(), LOS_NO_TRANS); i; ++i)
    {
        monster* target = monster_at(*i);
        if (!can_spawn_mushrooms(*i))
            continue;

        if (target && target->type != MONS_TOADSTOOL)
        {
            bool piety = !target->is_summoned();
            switch (mons_genus(target->mons_species()))
            {
            case MONS_ZOMBIE:
                // Maybe turn a zombie into a skeleton.
                if (mons_skeleton(mons_zombie_base(target)))
                {
                    simple_monster_message(target, "'s flesh rots away.");

                    downgrade_zombie_to_skeleton(target);

                    behaviour_event(target, ME_ALERT, &you);

                    if (piety)
                        processed_count++;

                    continue;
                }
                // Else fall through and destroy the zombie.
                // Ghoul-type monsters are always destroyed.
            case MONS_GHOUL:
            {
                simple_monster_message(target, " rots away and dies.");

                kills = true;

                const coord_def pos = target->pos();
                const int colour = target->colour;
                const int corpse = monster_die(target, KILL_MISC, NON_MONSTER,
                                               true);

                // If a corpse didn't drop, create a toadstool.
                // If one did drop, we will create toadstools from it as usual
                // later on.
                // Give neither piety nor toadstools for summoned creatures.
                // Assumes that summoned creatures do not drop corpses (hence
                // will not give piety in the next loop).
                if (corpse < 0 && piety)
                {
                    if (create_monster(
                                mgen_data(MONS_TOADSTOOL,
                                          BEH_GOOD_NEUTRAL,
                                          &you,
                                          0,
                                          0,
                                          pos,
                                          MHITNOT,
                                          MG_FORCE_PLACE,
                                          GOD_NO_GOD,
                                          MONS_NO_MONSTER,
                                          0,
                                          colour),
                                          false))
                    {
                        seen_mushrooms++;
                    }

                    processed_count++;
                    continue;
                }

                // Verify that summoned creatures do not drop a corpse.
                ASSERT(corpse < 0 || piety);

                break;
            }

            default:
                continue;
            }
        }

        for (stack_iterator j(*i); j; ++j)
        {
            bool corpse_on_pos = false;

            if (j->base_type == OBJ_CORPSES && j->sub_type == CORPSE_BODY)
            {
                corpse_on_pos = true;

                const int trial_prob = mushroom_prob(*j);
                const int target_count = 1 + binomial_generator(20, trial_prob);
                int seen_per;
                spawn_corpse_mushrooms(*j, target_count, seen_per,
                                       BEH_GOOD_NEUTRAL, true);

                // Either turn this corpse into a skeleton or destroy it.
                if (mons_skeleton(j->mon_type))
                    turn_corpse_into_skeleton(*j);
                else
                {
                    item_was_destroyed(*j);
                    destroy_item(j->index());
                }

                seen_mushrooms += seen_per;

                processed_count++;
            }

            if (corpse_on_pos && you.see_cell(*i))
                seen_corpses++;
        }
    }

    if (seen_mushrooms > 0)
        mushroom_spawn_message(seen_mushrooms, seen_corpses);

    if (kills)
        mpr("That felt like a moral victory.");

    if (processed_count)
    {
        simple_god_message(" appreciates your contribution to the "
                           "ecosystem.");
        // Doubling the expected value per sacrifice to approximate the
        // extra piety gain blood god worshipers get for the initial kill.
        // -cao

        int piety_gain = 0;
        for (int i = 0; i < processed_count * 2; ++i)
            piety_gain += random2(15); // avg 1.4 piety per corpse
        gain_piety(piety_gain, 10);
    }

    return processed_count;
}

static bool _create_plant(coord_def& target, int hp_adjust = 0)
{
    if (actor_at(target) || !mons_class_can_pass(MONS_PLANT, grd(target)))
        return 0;

    if (monster *plant = create_monster(mgen_data(MONS_PLANT,
                                            BEH_FRIENDLY,
                                            &you,
                                            0,
                                            0,
                                            target,
                                            MHITNOT,
                                            MG_FORCE_PLACE,
                                            GOD_FEDHAS)))
    {
        plant->flags |= MF_NO_REWARD;
        plant->flags |= MF_ATT_CHANGE_ATTEMPT;

        mons_make_god_gift(plant, GOD_FEDHAS);

        plant->max_hit_points += hp_adjust;
        plant->hit_points += hp_adjust;

        if (you.see_cell(target))
        {
            if (hp_adjust)
            {
                mprf("A plant, strengthened by %s, grows up from the ground.",
                     god_name(GOD_FEDHAS).c_str());
            }
            else
                mpr("A plant grows up from the ground.");
        }
        return true;
    }

    return false;
}

#define SUNLIGHT_DURATION 80

spret_type fedhas_sunlight(bool fail)
{
    dist spelld;

    bolt temp_bolt;
    temp_bolt.colour = YELLOW;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_HOSTILE_SUBMERGED;
    args.range = LOS_RADIUS;
    args.needs_path = false;
    args.may_target_monster = false;
    args.top_prompt = "Select sunlight destination.";
    direction(spelld, args);

    if (!spelld.isValid)
        return SPRET_ABORT;

    fail_check();

    const coord_def base = spelld.target;

    int revealed_count = 0;

    for (adjacent_iterator ai(base, false); ai; ++ai)
    {
        if (!in_bounds(*ai) || cell_is_solid(*ai))
            continue;

        for (size_t i = 0; i < env.sunlight.size(); ++i)
            if (env.sunlight[i].first == *ai)
            {
                erase_any(env.sunlight, i);
                break;
            }
        env.sunlight.push_back(pair<coord_def, int>(*ai,
            you.elapsed_time + (distance2(*ai, base) <= 1 ? SUNLIGHT_DURATION
                                : SUNLIGHT_DURATION / 2)));

        temp_bolt.explosion_draw_cell(*ai);

        monster *victim = monster_at(*ai);
        if (victim && you.see_cell(*ai) && !victim->visible_to(&you))
        {
            // Like entering/exiting angel halos, flipping autopickup would
            // be probably too much hassle.
            revealed_count++;
        }

        if (victim)
            behaviour_event(victim, ME_ALERT, &you);
    }

    {
        unwind_var<int> no_time(you.time_taken, 0);
        process_sunlights(false);
    }

#ifndef USE_TILE_LOCAL
    // Move the cursor out of the way (it looks weird).
    coord_def temp = grid2view(base);
    cgotoxy(temp.x, temp.y, GOTO_DNGN);
#endif
    scaled_delay(200);

    if (revealed_count)
    {
        mprf("In the bright light, you notice %s.", revealed_count == 1 ?
             "an invisible shape" : "some invisible shapes");
    }

    return SPRET_SUCCESS;
}

void process_sunlights(bool future)
{
    int time_cap = future ? INT_MAX - SUNLIGHT_DURATION : you.elapsed_time;

    int evap_count = 0;

    for (int i = env.sunlight.size() - 1; i >= 0; --i)
    {
        coord_def c = env.sunlight[i].first;
        int until = env.sunlight[i].second;

        if (until <= time_cap)
            erase_any(env.sunlight, i);

        until = min(until, time_cap);
        int from = you.elapsed_time - you.time_taken;

        // Deterministic roll, to guarantee evaporation when shined long enough.
        struct { level_id place; coord_def coord; int64_t game_start; } to_hash;
        to_hash.place = level_id::current();
        to_hash.coord = c;
        to_hash.game_start = you.birth_time;
        int h = hash32(&to_hash, sizeof(to_hash)) % SUNLIGHT_DURATION;

        if ((from + h) / SUNLIGHT_DURATION == (until + h) / SUNLIGHT_DURATION)
            continue;

        // Anything further on goes only on a successful evaporation roll, at
        // most once peer coord per invocation.

        // If this is a water square we will evaporate it.
        dungeon_feature_type ftype = grd(c);
        dungeon_feature_type orig_type = ftype;

        switch (ftype)
        {
        case DNGN_SHALLOW_WATER:
            ftype = DNGN_FLOOR;
            break;

        case DNGN_DEEP_WATER:
            ftype = DNGN_SHALLOW_WATER;
            break;

        default:
            break;
        }

        if (orig_type != ftype)
        {
            dungeon_terrain_changed(c, ftype);

            if (you.see_cell(c))
                evap_count++;

            // This is a little awkward but if we evaporated all the way to
            // the dungeon floor that may have given a monster
            // ENCH_AQUATIC_LAND, and if that happened the player should get
            // credit if the monster dies. The enchantment is inflicted via
            // the dungeon_terrain_changed call chain and that doesn't keep
            // track of what caused the terrain change. -cao
            monster* mons = monster_at(c);
            if (mons && ftype == DNGN_FLOOR
                && mons->has_ench(ENCH_AQUATIC_LAND))
            {
                mon_enchant temp = mons->get_ench(ENCH_AQUATIC_LAND);
                temp.who = KC_YOU;
                temp.source = MID_PLAYER;
                mons->add_ench(temp);
            }
        }
    }

    if (evap_count)
        mpr("Some water evaporates in the bright sunlight.");

    invalidate_agrid(true);
}

template<typename T>
static bool less_second(const T & left, const T & right)
{
    return left.second < right.second;
}

typedef pair<coord_def, int> point_distance;

// Find the distance from origin to each of the targets, those results
// are stored in distances (which is the same size as targets). Exclusion
// is a set of points which are considered disconnected for the search.
static void _path_distance(const coord_def& origin,
                           const vector<coord_def>& targets,
                           set<int> exclusion,
                           vector<int>& distances)
{
    queue<point_distance> fringe;
    fringe.push(point_distance(origin,0));
    distances.clear();
    distances.resize(targets.size(), INT_MAX);

    while (!fringe.empty())
    {
        point_distance current = fringe.front();
        fringe.pop();

        // did we hit a target?
        for (unsigned i = 0; i < targets.size(); ++i)
        {
            if (current.first == targets[i])
            {
                distances[i] = current.second;
                break;
            }
        }

        for (adjacent_iterator adj_it(current.first); adj_it; ++adj_it)
        {
            int idx = adj_it->x + adj_it->y * X_WIDTH;
            if (you.see_cell(*adj_it)
                && !feat_is_solid(env.grid(*adj_it))
                && *adj_it != you.pos()
                && exclusion.insert(idx).second)
            {
                monster* temp = monster_at(*adj_it);
                if (!temp || (temp->attitude == ATT_HOSTILE
                              && !temp->is_stationary()))
                {
                    fringe.push(point_distance(*adj_it, current.second+1));
                }
            }
        }
    }
}

// Find the minimum distance from each point of origin to one of the targets
// The distance is stored in 'distances', which is the same size as origins.
static void _point_point_distance(const vector<coord_def>& origins,
                                  const vector<coord_def>& targets,
                                  vector<int>& distances)
{
    distances.clear();
    distances.resize(origins.size(), INT_MAX);

    // Consider all points of origin as blocked (you can search outward
    // from one, but you can't form a path across a different one).
    set<int> base_exclusions;
    for (unsigned i = 0; i < origins.size(); ++i)
    {
        int idx = origins[i].x + origins[i].y * X_WIDTH;
        base_exclusions.insert(idx);
    }

    vector<int> current_distances;
    for (unsigned i = 0; i < origins.size(); ++i)
    {
        // Find the distance from the point of origin to each of the targets.
        _path_distance(origins[i], targets, base_exclusions,
                       current_distances);

        // Find the smallest of those distances
        int min_dist = current_distances[0];
        for (unsigned j = 1; j < current_distances.size(); ++j)
            if (current_distances[j] < min_dist)
                min_dist = current_distances[j];

        distances[i] = min_dist;
    }
}

// So the idea is we want to decide which adjacent tiles are in the most 'danger'
// We claim danger is proportional to the minimum distances from the point to a
// (hostile) monster. This function carries out at most 7 searches to calculate
// the distances in question.
bool prioritise_adjacent(const coord_def &target, vector<coord_def>& candidates)
{
    radius_iterator los_it(target, LOS_NO_TRANS, true);

    vector<coord_def> mons_positions;
    // collect hostile monster positions in LOS
    for (; los_it; ++los_it)
    {
        monster* hostile = monster_at(*los_it);

        if (hostile && hostile->attitude == ATT_HOSTILE
            && you.can_see(hostile))
        {
            mons_positions.push_back(hostile->pos());
        }
    }

    if (mons_positions.empty())
    {
        shuffle_array(candidates);
        return true;
    }

    vector<int> distances;

    _point_point_distance(candidates, mons_positions, distances);

    vector<point_distance> possible_moves(candidates.size());

    for (unsigned i = 0; i < possible_moves.size(); ++i)
    {
        possible_moves[i].first  = candidates[i];
        possible_moves[i].second = distances[i];
    }

    sort(possible_moves.begin(), possible_moves.end(),
              less_second<point_distance>);

    for (unsigned i = 0; i < candidates.size(); ++i)
        candidates[i] = possible_moves[i].first;

    return true;
}

static bool _prompt_amount(int max, int& selected, const string& prompt)
{
    selected = max;
    while (true)
    {
        msg::streams(MSGCH_PROMPT) << prompt << " (" << max << " max) " << endl;

        const int keyin = get_ch();

        // Cancel
        if (key_is_escape(keyin) || keyin == ' ' || keyin == '0')
        {
            canned_msg(MSG_OK);
            return false;
        }

        // Default is max
        if (keyin == '\n' || keyin == '\r')
        {
            selected = max;
            return true;
        }

        // Otherwise they should enter a digit
        if (isadigit(keyin))
        {
            selected = keyin - '0';
            if (selected > 0 && selected <= max)
                return true;
        }
        // else they entered some garbage?
    }

    return false;
}

static int _collect_fruit(vector<pair<int,int> >& available_fruit)
{
    int total = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].defined() && is_fruit(you.inv[i]))
        {
            total += you.inv[i].quantity;
            available_fruit.push_back(make_pair(you.inv[i].quantity, i));
        }
    }
    sort(available_fruit.begin(), available_fruit.end());

    return total;
}

static void _decrease_amount(vector<pair<int, int> >& available, int amount)
{
    const int total_decrease = amount;
    for (unsigned int i = 0; i < available.size() && amount > 0; i++)
    {
        const int decrease_amount = min(available[i].first, amount);
        amount -= decrease_amount;
        dec_inv_item_quantity(available[i].second, decrease_amount);
    }
    if (total_decrease > 1)
        mprf("%d pieces of fruit are consumed!", total_decrease);
    else
        mpr("A piece of fruit is consumed!");
}

// Create a ring or partial ring around the caster.  The user is
// prompted to select a stack of fruit, and then plants are placed on open
// squares adjacent to the user.  Of course, one piece of fruit is
// consumed per plant, so a complete ring may not be formed.
bool fedhas_plant_ring_from_fruit()
{
    // How much fruit is available?
    vector<pair<int, int> > collected_fruit;
    int total_fruit = _collect_fruit(collected_fruit);

    // How many adjacent open spaces are there?
    vector<coord_def> adjacent;
    for (adjacent_iterator adj_it(you.pos()); adj_it; ++adj_it)
    {
        if (monster_habitable_grid(MONS_PLANT, env.grid(*adj_it))
            && !actor_at(*adj_it))
        {
            adjacent.push_back(*adj_it);
        }
    }

    const int max_use = min(total_fruit, static_cast<int>(adjacent.size()));

    // Don't prompt if we can't do anything (due to having no fruit or
    // no squares to place plants on).
    if (max_use == 0)
    {
        if (adjacent.empty())
            mpr("No empty adjacent squares.");
        else
            mpr("No fruit available.");

        return false;
    }

    prioritise_adjacent(you.pos(), adjacent);

    // Screwing around with display code I don't really understand. -cao
    targetter_smite range(&you, 1);
    range_view_annotator show_range(&range);

    for (int i = 0; i < max_use; ++i)
    {
#ifndef USE_TILE_LOCAL
        coord_def temp = grid2view(adjacent[i]);
        cgotoxy(temp.x, temp.y, GOTO_DNGN);
        put_colour_ch(GREEN, '1' + i);
#endif
#ifdef USE_TILE
        tiles.add_overlay(adjacent[i], TILE_INDICATOR + i);
#endif
    }

    // And how many plants does the user want to create?
    int target_count;
    if (!_prompt_amount(max_use, target_count,
                        "How many plants will you create?"))
    {
        // User cancelled at the prompt.
        return false;
    }

    const int hp_adjust = you.skill(SK_INVOCATIONS, 10);

    // The user entered a number, remove all number overlays which
    // are higher than that number.
#ifndef USE_TILE_LOCAL
    unsigned not_used = adjacent.size() - unsigned(target_count);
    for (unsigned i = adjacent.size() - not_used; i < adjacent.size(); i++)
        view_update_at(adjacent[i]);
#endif
#ifdef USE_TILE
    // For tiles we have to clear all overlays and redraw the ones
    // we want.
    tiles.clear_overlays();
    for (int i = 0; i < target_count; ++i)
        tiles.add_overlay(adjacent[i], TILE_INDICATOR + i);
#endif

    int created_count = 0;
    for (int i = 0; i < target_count; ++i)
    {
        if (_create_plant(adjacent[i], hp_adjust))
            created_count++;

        // Clear the overlay and draw the plant we just placed.
        // This is somewhat more complicated in tiles.
        view_update_at(adjacent[i]);
#ifdef USE_TILE
        tiles.clear_overlays();
        for (int j = i + 1; j < target_count; ++j)
            tiles.add_overlay(adjacent[j], TILE_INDICATOR + j);
        viewwindow(false);
#endif
        scaled_delay(200);
    }

    if (created_count)
        _decrease_amount(collected_fruit, created_count);
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return created_count;
}

// Create a circle of water around the target, with a radius of
// approximately 2.  This turns normal floor tiles into shallow water
// and turns (unoccupied) shallow water into deep water.  There is a
// chance of spawning plants or fungus on unoccupied dry floor tiles
// outside of the rainfall area.  Return the number of plants/fungi
// created.
int fedhas_rain(const coord_def &target)
{
    int spawned_count = 0;
    int processed_count = 0;

    for (radius_iterator rad(target, LOS_NO_TRANS, true); rad; ++rad)
    {
        // Adjust the shape of the rainfall slightly to make it look
        // nicer.  I want a threshold of 2.5 on the euclidean distance,
        // so a threshold of 6 prior to the sqrt is close enough.
        int rain_thresh = 6;
        coord_def local = *rad - target;

        dungeon_feature_type ftype = grd(*rad);

        if (local.abs() > rain_thresh)
        {
            // Maybe spawn a plant on (dry, open) squares that are in
            // LOS but outside the rainfall area.  In open space, there
            // are 213 squares in LOS, and we are going to drop water on
            // (25-4) of those, so if we want x plants to spawn on
            // average in open space, the trial probability should be
            // x/192.
            if (x_chance_in_y(5, 192)
                && !actor_at(*rad)
                && ftype == DNGN_FLOOR)
            {
                if (create_monster(mgen_data(
                                      coinflip() ? MONS_PLANT : MONS_FUNGUS,
                                      BEH_GOOD_NEUTRAL,
                                      &you,
                                      0,
                                      0,
                                      *rad,
                                      MHITNOT,
                                      MG_FORCE_PLACE,
                                      GOD_FEDHAS)))
                {
                    spawned_count++;
                }

                processed_count++;
            }

            continue;
        }

        // Turn regular floor squares only into shallow water.
        if (ftype == DNGN_FLOOR)
        {
            dungeon_terrain_changed(*rad, DNGN_SHALLOW_WATER);

            processed_count++;
        }
        // We can also turn shallow water into deep water, but we're
        // just going to skip cases where there is something on the
        // shallow water.  Destroying items will probably be annoying,
        // and insta-killing monsters is clearly out of the question.
        else if (!actor_at(*rad)
                 && igrd(*rad) == NON_ITEM
                 && ftype == DNGN_SHALLOW_WATER)
        {
            dungeon_terrain_changed(*rad, DNGN_DEEP_WATER);

            processed_count++;
        }

        if (!feat_is_solid(ftype))
        {
            // Maybe place a raincloud.
            //
            // The rainfall area is 20 (5*5 - 4 (corners) - 1 (center));
            // the expected number of clouds generated by a fixed chance
            // per tile is 20 * p = expected.  Say an Invocations skill
            // of 27 gives expected 5 clouds.
            int max_expected = 5;
            int expected = you.skill_rdiv(SK_INVOCATIONS, max_expected, 27);

            if (x_chance_in_y(expected, 20))
            {
                place_cloud(CLOUD_RAIN, *rad, 10, &you);

                processed_count++;
            }
        }
    }

    if (spawned_count > 0)
    {
        mprf("%s grow%s in the rain.",
             (spawned_count > 1 ? "Some plants" : "A plant"),
             (spawned_count > 1 ? "" : "s"));
    }

    return processed_count;
}

int count_corpses_in_los(vector<stack_iterator> *positions)
{
    int count = 0;

    for (radius_iterator rad(you.pos(), LOS_NO_TRANS, true); rad;
         ++rad)
    {
        if (actor_at(*rad))
            continue;

        for (stack_iterator stack_it(*rad); stack_it; ++stack_it)
        {
            if (stack_it->base_type == OBJ_CORPSES
                && stack_it->sub_type == CORPSE_BODY)
            {
                if (positions)
                    positions->push_back(stack_it);
                count++;
                break;
            }
        }
    }

    return count;
}

int fedhas_check_corpse_spores(bool quiet)
{
    vector<stack_iterator> positions;
    int count = count_corpses_in_los(&positions);

    if (quiet || count == 0)
        return count;

    viewwindow(false);
    for (unsigned i = 0; i < positions.size(); ++i)
    {
#ifndef USE_TILE_LOCAL
        coord_def temp = grid2view(positions[i]->pos);
        cgotoxy(temp.x, temp.y, GOTO_DNGN);

        unsigned colour = GREEN | COLFLAG_FRIENDLY_MONSTER;
        colour = real_colour(colour);

        unsigned character = mons_char(MONS_GIANT_SPORE);
        put_colour_ch(colour, character);
#endif
#ifdef USE_TILE
        tiles.add_overlay(positions[i]->pos, TILE_SPORE_OVERLAY);
#endif
    }

    if (yesnoquit("Will you create these spores?", true, 'y') <= 0)
    {
        viewwindow(false);
        return -1;
    }

    return count;
}

// Destroy corpses in the player's LOS (first corpse on a stack only)
// and make 1 giant spore per corpse.  Spores are given the input as
// their starting behavior; the function returns the number of corpses
// processed.
int fedhas_corpse_spores(beh_type attitude)
{
    vector<stack_iterator> positions;
    int count = count_corpses_in_los(&positions);
    ASSERT(attitude != BEH_FRIENDLY || count > 0);

    if (count == 0)
        return count;

    for (unsigned i = 0; i < positions.size(); ++i)
    {
        count++;

        if (monster *plant = create_monster(mgen_data(MONS_GIANT_SPORE,
                                               attitude,
                                               &you,
                                               0,
                                               0,
                                               positions[i]->pos,
                                               MHITNOT,
                                               MG_FORCE_PLACE,
                                               GOD_FEDHAS)))
        {
            plant->flags |= MF_NO_REWARD;

            if (attitude == BEH_FRIENDLY)
            {
                plant->flags |= MF_ATT_CHANGE_ATTEMPT;

                mons_make_god_gift(plant, GOD_FEDHAS);

                plant->behaviour = BEH_WANDER;
                plant->foe = MHITNOT;
            }
        }

        if (mons_skeleton(positions[i]->mon_type))
            turn_corpse_into_skeleton(*positions[i]);
        else
        {
            item_was_destroyed(*positions[i]);
            destroy_item(positions[i]->index());
        }
    }

    viewwindow(false);

    return count;
}

struct monster_conversion
{
    monster_conversion() :
        base_monster(NULL),
        piety_cost(0),
        fruit_cost(0)
    {
    }

    monster* base_monster;
    int piety_cost;
    int fruit_cost;
    monster_type new_type;
};

// Given a monster (which should be a plant/fungus), see if
// fedhas_evolve_flora() can upgrade it, and set up a monster_conversion
// structure for it.  Return true (and fill in possible_monster) if the
// monster can be upgraded, and return false otherwise.
static bool _possible_evolution(const monster* input,
                                monster_conversion& possible_monster)
{
    switch (input->type)
    {
    case MONS_PLANT:
    case MONS_BUSH:
    case MONS_BURNING_BUSH:
        possible_monster.new_type = MONS_OKLOB_PLANT;
        possible_monster.fruit_cost = 1;
        break;

    case MONS_OKLOB_SAPLING:
        possible_monster.new_type = MONS_OKLOB_PLANT;
        possible_monster.piety_cost = 4;
        break;

    case MONS_FUNGUS:
    case MONS_TOADSTOOL:
        possible_monster.new_type = MONS_WANDERING_MUSHROOM;
        possible_monster.piety_cost = 3;
        break;

    case MONS_BALLISTOMYCETE:
        possible_monster.new_type = MONS_HYPERACTIVE_BALLISTOMYCETE;
        possible_monster.piety_cost = 4;
        break;

    default:
        return false;
    }

    return true;
}

bool mons_is_evolvable(const monster* mon)
{
    monster_conversion temp;
    return _possible_evolution(mon, temp);
}

static bool _place_ballisto(const coord_def& pos)
{
    if (monster *plant = create_monster(mgen_data(MONS_BALLISTOMYCETE,
                                                      BEH_FRIENDLY,
                                                      &you,
                                                      0,
                                                      0,
                                                      pos,
                                                      MHITNOT,
                                                      MG_FORCE_PLACE,
                                                      GOD_FEDHAS)))
    {
        plant->flags |= MF_NO_REWARD;
        plant->flags |= MF_ATT_CHANGE_ATTEMPT;

        mons_make_god_gift(plant, GOD_FEDHAS);

        remove_mold(pos);
        mpr("The mold grows into a ballistomycete.");
        mpr("Your piety has decreased.");
        lose_piety(1);
        return true;
    }

    // Monster placement failing should be quite unusual, but it could happen.
    // Not entirely sure what to say about it, but a more informative message
    // might be good. -cao
    canned_msg(MSG_NOTHING_HAPPENS);
    return false;
}

#define FEDHAS_EVOLVE_TARGET_KEY "fedhas_evolve_target"

bool fedhas_check_evolve_flora(bool quiet)
{
    monster_conversion upgrade;

    // This is a little sloppy, but cancel early if nothing useful is in
    // range.
    bool in_range = false;
    for (radius_iterator rad(you.pos(), LOS_NO_TRANS, true); rad; ++rad)
    {
        const monster* temp = monster_at(*rad);
        if (is_moldy(*rad) && mons_class_can_pass(MONS_BALLISTOMYCETE,
                                                  env.grid(*rad))
            || temp && mons_is_evolvable(temp))
        {
            in_range = true;
            break;
        }
    }

    if (!in_range)
    {
        if (!quiet)
            mpr("No evolvable flora in sight.");
        return false;
    }

    if (quiet) // just checking if there's something we can evolve here
        return true;

    dist spelld;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_EVOLVABLE_PLANTS;
    args.range = LOS_RADIUS;
    args.needs_path = false;
    args.may_target_monster = false;
    args.cancel_at_self = true;
    args.show_floor_desc = true;
    args.top_prompt = "Select plant or fungus to evolve.";

    direction(spelld, args);

    if (!spelld.isValid)
    {
        // Check for user cancel.
        canned_msg(MSG_OK);
        return false;
    }

    monster* const plant = monster_at(spelld.target);

    if (!plant)
    {
        if (!is_moldy(spelld.target)
            || !mons_class_can_pass(MONS_BALLISTOMYCETE,
                                    env.grid(spelld.target)))
        {
            if (feat_is_tree(env.grid(spelld.target)))
                mpr("The tree has already reached the pinnacle of evolution.");
            else
                mpr("You must target a plant or fungus.");
            return false;
        }
        you.props[FEDHAS_EVOLVE_TARGET_KEY].get_coord() = spelld.target;
        return true;
    }

    if (!_possible_evolution(plant, upgrade))
    {
        if (plant->type == MONS_GIANT_SPORE)
            mpr("You can evolve only complete plants, not seeds.");
        else  if (mons_is_plant(plant))
        {
            simple_monster_message(plant, " has already reached the pinnacle"
                                   " of evolution.");
        }
        else
            mpr("Only plants or fungi may be evolved.");

        return false;
    }

    vector<pair<int, int> > collected_fruit;
    if (upgrade.fruit_cost)
    {
        const int total_fruit = _collect_fruit(collected_fruit);

        if (total_fruit < upgrade.fruit_cost)
        {
            mpr("Not enough fruit available.");
            return false;
        }
    }

    if (upgrade.piety_cost && upgrade.piety_cost > you.piety)
    {
        mpr("Not enough piety available.");
        return false;
    }

    you.props[FEDHAS_EVOLVE_TARGET_KEY].get_coord() = spelld.target;
    return true;
}

void fedhas_evolve_flora()
{
    monster_conversion upgrade;
    const coord_def target = you.props[FEDHAS_EVOLVE_TARGET_KEY].get_coord();
    you.props.erase(FEDHAS_EVOLVE_TARGET_KEY);

    monster* const plant = monster_at(target);

    if (!plant)
    {
        ASSERT(is_moldy(target));
        _place_ballisto(target);
        return;
    }

    ASSERT(_possible_evolution(plant, upgrade));

    switch (plant->type)
    {
    case MONS_PLANT:
    case MONS_BUSH:
    {
        string evolve_desc = " can now spit acid";
        int skill = you.skill(SK_INVOCATIONS);
        if (skill >= 20)
            evolve_desc += " continuously";
        else if (skill >= 15)
            evolve_desc += " quickly";
        else if (skill >= 10)
            evolve_desc += " rather quickly";
        else if (skill >= 5)
            evolve_desc += " somewhat quickly";
        evolve_desc += ".";

        simple_monster_message(plant, evolve_desc.c_str());
        break;
    }

    case MONS_OKLOB_SAPLING:
        simple_monster_message(plant, " appears stronger.");
        break;

    case MONS_FUNGUS:
    case MONS_TOADSTOOL:
        simple_monster_message(plant, " can now pick up its mycelia and move.");
        break;

    case MONS_BALLISTOMYCETE:
        simple_monster_message(plant, " appears agitated.");
        env.level_state |= LSTATE_GLOW_MOLD;
        break;

    default:
        break;
    }

    plant->upgrade_type(upgrade.new_type, true, true);

    plant->flags |= MF_NO_REWARD;
    plant->flags |= MF_ATT_CHANGE_ATTEMPT;

    mons_make_god_gift(plant, GOD_FEDHAS);

    plant->attitude = ATT_FRIENDLY;

    behaviour_event(plant, ME_ALERT);
    mons_att_changed(plant);

    // Try to remove slowly dying in case we are upgrading a
    // toadstool, and spore production in case we are upgrading a
    // ballistomycete.
    plant->del_ench(ENCH_SLOWLY_DYING);
    plant->del_ench(ENCH_SPORE_PRODUCTION);

    if (plant->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
        plant->add_ench(ENCH_EXPLODING);
    else if (plant->type == MONS_OKLOB_PLANT)
    {
        plant->spells.clear();
        plant->spells.push_back(mon_spell_slot());
        plant->spells[0].spell = SPELL_SPIT_ACID;
        plant->spells[0].flags = MON_SPELL_NATURAL;
    }

    plant->set_hit_dice(plant->get_experience_level()
                        + you.skill_rdiv(SK_INVOCATIONS));

    if (upgrade.fruit_cost)
    {
        vector<pair<int, int> > collected_fruit;
        _collect_fruit(collected_fruit);
        _decrease_amount(collected_fruit, upgrade.fruit_cost);
    }

    if (upgrade.piety_cost)
    {
        lose_piety(upgrade.piety_cost);
        mpr("Your piety has decreased.");
    }
}

static int _lugonu_warp_monster(monster* mon, int pow)
{
    if (mon == NULL)
        return 0;

    if (coinflip())
        return 0;

    if (!mon->friendly())
        behaviour_event(mon, ME_ANNOY, &you);

    const int damage = 1 + random2(pow / 6);
    if (mons_genus(mon->type) == MONS_BLINK_FROG)
    {
        mprf("%s basks in the distortional energy.",
             mon->name(DESC_THE).c_str());
        mon->heal(damage, false);
    }
    else
    {
        mon->hurt(&you, damage);
        if (!mon->alive())
            return 1;
    }

    if (!mon->no_tele(true, false))
        mon->blink();

    return 1;
}

static void _lugonu_warp_area(int pow)
{
    apply_monsters_around_square(_lugonu_warp_monster, you.pos(), pow);
}

void lugonu_bend_space()
{
    const int pow = 4 + skill_bump(SK_INVOCATIONS);
    const bool area_warp = random2(pow) > 9;

    mprf("Space bends %saround you!", area_warp ? "sharply " : "");

    if (area_warp)
        _lugonu_warp_area(pow);

    random_blink(false, true, true);

    const int damage = roll_dice(1, 4);
    ouch(damage, KILLED_BY_WILD_MAGIC, MID_NOBODY, "a spatial distortion");
}

void cheibriados_time_bend(int pow)
{
    mpr("The flow of time bends around you.");

    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        monster* mon = monster_at(*ai);
        if (mon && !mon->is_stationary())
        {
            int res_margin = roll_dice(mon->get_hit_dice(), 3)
                             - random2avg(pow, 2);
            if (res_margin > 0)
            {
                mprf("%s%s",
                     mon->name(DESC_THE).c_str(),
                     resist_margin_phrase(res_margin));
                continue;
            }

            simple_god_message(
                make_stringf(" rebukes %s.",
                             mon->name(DESC_THE).c_str()).c_str(),
                             GOD_CHEIBRIADOS);
            do_slow_monster(mon, &you);
        }
    }
}

static int _slouchable(coord_def where, int pow, int, actor* agent)
{
    monster* mon = monster_at(where);
    if (mon == NULL || mon->is_stationary() || mon->cannot_move()
        || mons_is_projectile(mon->type)
        || mon->asleep() && !mons_is_confused(mon))
    {
        return 0;
    }

    int dmg = (mon->speed - 1000/player_movement_speed()/player_speed());
    return (dmg > 0) ? 1 : 0;
}

static bool _act_slouchable(const actor *act)
{
    if (act->is_player())
        return false;  // too slow-witted
    return _slouchable(act->pos(), 0, 0, 0);
}

static int _slouch_monsters(coord_def where, int pow, int dummy, actor* agent)
{
    if (!_slouchable(where, pow, dummy, agent))
        return 0;

    monster* mon = monster_at(where);
    ASSERT(mon);

    int dmg = (mon->speed - 1000/player_movement_speed()/player_speed());
    dmg = (dmg > 0 ? roll_dice(dmg*4, 3)/2 : 0);

    mon->hurt(agent, dmg, BEAM_MMISSILE, true);
    return 1;
}

bool cheibriados_slouch(int pow)
{
    int count = apply_area_visible(_slouchable, pow, &you);
    if (!count)
        if (!yesno("There's no one hasty visible. Invoke Slouch anyway?",
                   true, 'n'))
        {
            return false;
        }

    targetter_los hitfunc(&you, LOS_DEFAULT);
    if (stop_attack_prompt(hitfunc, "harm", _act_slouchable))
        return false;

    mpr("You can feel time thicken for a moment.");
    dprf("your speed is %d", player_movement_speed());

    apply_area_visible(_slouch_monsters, pow, &you);
    return true;
}

// A low-duration step from time, allowing monsters to get closer
// to the player safely.
void cheibriados_temporal_distortion()
{
    const coord_def old_pos = you.pos();

    const int time = 3 + random2(3);
    you.moveto(coord_def(0, 0));
    you.duration[DUR_TIME_STEP] = time;

    do
    {
        run_environment_effects();
        handle_monsters();
        manage_clouds();
    }
    while (--you.duration[DUR_TIME_STEP] > 0);

    monster* mon;
    if (mon = monster_at(old_pos))
    {
        mon->blink();
        if (mon = monster_at(old_pos))
            mon->teleport(true);
    }

    you.moveto(old_pos);
    you.duration[DUR_TIME_STEP] = 0;

    mpr("You warp the flow of time around you!");
}

void cheibriados_time_step(int pow) // pow is the number of turns to skip
{
    const coord_def old_pos = you.pos();

    mpr("You step out of the flow of time.");
    flash_view(UA_PLAYER, LIGHTBLUE);
    you.moveto(coord_def(0, 0));
    you.duration[DUR_TIME_STEP] = pow;

    you.time_taken = 10;
    do
    {
        run_environment_effects();
        handle_monsters();
        manage_clouds();
    }
    while (--you.duration[DUR_TIME_STEP] > 0);
    // Update corpses, etc.  This does also shift monsters, but only by
    // a tiny bit.
    update_level(pow * 10);

#ifndef USE_TILE_LOCAL
    scaled_delay(1000);
#endif

    monster* mon;
    if (mon = monster_at(old_pos))
    {
        mon->blink();
        if (mon = monster_at(old_pos))
            mon->teleport(true);
    }

    you.moveto(old_pos);
    you.duration[DUR_TIME_STEP] = 0;

    flash_view(UA_PLAYER, 0);
    mpr("You return to the normal time flow.");
}

bool ashenzari_transfer_knowledge()
{
    if (you.transfer_skill_points > 0 && !ashenzari_end_transfer())
        return false;

    while (true)
    {
        skill_menu(SKMF_RESKILL_FROM);
        if (is_invalid_skill(you.transfer_from_skill))
        {
            redraw_screen();
            return false;
        }

        you.transfer_skill_points = skill_transfer_amount(
                                                    you.transfer_from_skill);

        skill_menu(SKMF_RESKILL_TO);
        if (is_invalid_skill(you.transfer_to_skill))
        {
            you.transfer_from_skill = SK_NONE;
            you.transfer_skill_points = 0;
            continue;
        }

        break;
    }

    // We reset the view to force view transfer next time.
    you.skill_menu_view = SKM_NONE;

    mprf("As you forget about %s, you feel ready to understand %s.",
         skill_name(you.transfer_from_skill),
         skill_name(you.transfer_to_skill));

    you.transfer_total_skill_points = you.transfer_skill_points;

    redraw_screen();
    return true;
}

bool ashenzari_end_transfer(bool finished, bool force)
{
    if (!force && !finished)
    {
        mprf("You are currently transferring knowledge from %s to %s.",
             skill_name(you.transfer_from_skill),
             skill_name(you.transfer_to_skill));
        if (!yesno("Are you sure you want to cancel the transfer?", false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    mprf("You %s forgetting about %s and learning about %s.",
         finished ? "have finished" : "stop",
         skill_name(you.transfer_from_skill),
         skill_name(you.transfer_to_skill));
    you.transfer_from_skill = SK_NONE;
    you.transfer_to_skill = SK_NONE;
    you.transfer_skill_points = 0;
    you.transfer_total_skill_points = 0;
    return true;
}

bool can_convert_to_beogh()
{
    if (silenced(you.pos()))
        return false;

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        const monster * const mon = monster_at(*ri);
        if (mons_allows_beogh_now(mon))
            return true;
    }

    return false;
}

void spare_beogh_convert()
{
    if (you.one_time_ability_used[GOD_BEOGH])
    {
        // You still get to convert, but orcs will remain hostile.
        mprf(MSGCH_TALK, "%s", getSpeakString("orc_priest_apostate").c_str());
        return;
    }

    set<mid_t> witnesses;

    you.religion = GOD_NO_GOD;
    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        const monster *mon = monster_at(*ri);
        // An invis player converting is ok, for simplicity.
        if (!mon || !cell_see_cell(you.pos(), *ri, LOS_DEFAULT))
            continue;
        if (mon->attitude != ATT_HOSTILE)
            continue;
        if (mons_genus(mon->type) != MONS_ORC)
            continue;
        witnesses.insert(mon->mid);

        // Anyone who has seen the priest perform the ceremony will spare you
        // as well.
        if (mons_allows_beogh(mon))
        {
            for (radius_iterator pi(you.pos(), LOS_DEFAULT); pi; ++pi)
            {
                const monster *orc = monster_at(*pi);
                if (!orc || !cell_see_cell(*ri, *pi, LOS_DEFAULT))
                    continue;
                if (mons_genus(orc->type) != MONS_ORC)
                    continue;
                if (mon->attitude != ATT_HOSTILE)
                    continue;
                witnesses.insert(orc->mid);
            }
        }
    }

    int witc = 0;
    for (auto wit : witnesses)
    {
        monster *orc = monster_by_mid(wit);
        if (!orc || !orc->alive())
            continue;

        ++witc;
        orc->del_ench(ENCH_CHARM);
        mons_pacify(orc, ATT_GOOD_NEUTRAL, true);
    }

    you.religion = GOD_BEOGH;
    you.one_time_ability_used.set(GOD_BEOGH);

    if (witc == 1)
        mpr("The priest welcomes you and lets you live.");
    else
    {
        mpr("With a roar of approval, the orcs welcome you as one of their own,"
            " and spare your life.");
    }
}

bool dithmenos_shadow_step()
{
    // You can shadow-step anywhere within your umbra.
    ASSERT(you.umbra_radius2() > -1);
    const int range = isqrt_ceil(you.umbra_radius2());

    targetter_shadow_step tgt(&you, you.umbra_radius2());
    direction_chooser_args args;
    args.hitfunc = &tgt;
    args.restricts = DIR_SHADOW_STEP;
    args.mode = TARG_HOSTILE;
    args.range = range;
    args.just_looking = false;
    args.needs_path = false;
    args.top_prompt = "Aiming: <white>Shadow Step</white>";
    dist sdirect;
    direction(sdirect, args);
    if (!sdirect.isValid || tgt.landing_site.origin())
        return false;

    // Check for hazards.
    bool zot_trap_prompted = false,
         trap_prompted = false,
         exclusion_prompted = false,
         cloud_prompted = false,
         terrain_prompted = false;

    for (auto site : tgt.additional_sites)
    {
        if (!cloud_prompted
            && !check_moveto_cloud(site, "shadow step", &cloud_prompted))
        {
            return false;
        }

        if (!zot_trap_prompted)
        {
            trap_def* trap = find_trap(site);
            if (trap && env.grid(site) != DNGN_UNDISCOVERED_TRAP
                && trap->type == TRAP_ZOT)
            {
                if (!check_moveto_trap(site, "shadow step",
                                       &trap_prompted))
                {
                    you.turn_is_over = false;
                    return false;
                }
                zot_trap_prompted = true;
            }
            else if (!trap_prompted
                     && !check_moveto_trap(site, "shadow step",
                                           &trap_prompted))
            {
                you.turn_is_over = false;
                return false;
            }
        }

        if (!exclusion_prompted
            && !check_moveto_exclusion(site, "shadow step",
                                       &exclusion_prompted))
        {
            return false;
        }

        if (!terrain_prompted
            && !check_moveto_terrain(site, "shadow step", "",
                                     &terrain_prompted))
        {
            return false;
        }
    }

    // XXX: This only ever fails if something's on the landing site;
    // perhaps this should be handled more gracefully.
    if (!you.move_to_pos(tgt.landing_site))
    {
        mpr("Something blocks your shadow step.");
        return true;
    }

    const actor *victim = actor_at(sdirect.target);
    mprf("You step into %s shadow.",
         apostrophise(victim->name(DESC_THE)).c_str());

    return true;
}

static bool _dithmenos_shadow_acts()
{
    if (!in_good_standing(GOD_DITHMENOS, 3))
        return false;

    // 10% chance at 4* piety; 50% chance at 200 piety.
    const int range = MAX_PIETY - piety_breakpoint(3);
    const int min   = range / 5;
    return x_chance_in_y(min + ((range - min)
                                * (you.piety - piety_breakpoint(3))
                                / (MAX_PIETY - piety_breakpoint(3))),
                         2 * range);
}

monster* shadow_monster(bool equip)
{
    if (monster_at(you.pos()))
        return NULL;

    int wpn_index  = NON_ITEM;

    // Do a basic clone of the weapon.
    item_def* wpn = you.weapon();
    if (equip
        && wpn
        && (wpn->base_type == OBJ_WEAPONS
            || wpn->base_type == OBJ_STAVES
            || wpn->base_type == OBJ_RODS))
    {
        wpn_index = get_mitm_slot(10);
        if (wpn_index == NON_ITEM)
            return NULL;
        item_def& new_item = mitm[wpn_index];
        if (wpn->base_type == OBJ_STAVES)
        {
            new_item.base_type = OBJ_WEAPONS;
            new_item.sub_type  = WPN_STAFF;
        }
        else if (wpn->base_type == OBJ_RODS)
        {
            new_item.base_type = OBJ_WEAPONS;
            new_item.sub_type  = WPN_ROD;
        }
        else
        {
            new_item.base_type = wpn->base_type;
            new_item.sub_type  = wpn->sub_type;
        }
        new_item.quantity = 1;
        new_item.rnd = 1;
        new_item.flags   |= ISFLAG_SUMMONED;
    }

    monster* mon = get_free_monster();
    if (!mon)
    {
        if (wpn_index)
            destroy_item(wpn_index);
        return NULL;
    }

    mon->type       = MONS_PLAYER_SHADOW;
    mon->behaviour  = BEH_SEEK;
    mon->attitude   = ATT_FRIENDLY;
    mon->flags      = MF_NO_REWARD | MF_JUST_SUMMONED | MF_SEEN
                    | MF_WAS_IN_VIEW | MF_HARD_RESET;
    mon->hit_points = you.hp;
    mon->set_hit_dice(min(27, max(1,
                                  you.skill_rdiv(wpn_index != NON_ITEM
                                                 ? item_attack_skill(mitm[wpn_index])
                                                 : SK_UNARMED_COMBAT, 10, 20)
                                  + you.skill_rdiv(SK_FIGHTING, 10, 20))));
    mon->set_position(you.pos());
    mon->mid        = MID_PLAYER;
    mon->inv[MSLOT_WEAPON]  = wpn_index;
    mon->inv[MSLOT_MISSILE] = NON_ITEM;

    mgrd(you.pos()) = mon->mindex();

    return mon;
}

void shadow_monster_reset(monster *mon)
{
    if (mon->inv[MSLOT_WEAPON] != NON_ITEM)
        destroy_item(mon->inv[MSLOT_WEAPON]);
    if (mon->inv[MSLOT_MISSILE] != NON_ITEM)
        destroy_item(mon->inv[MSLOT_MISSILE]);

    mon->reset();
}

/**
 * Check if the player is in melee range of the target.
 *
 * Certain effects, e.g. distortion blink, can cause monsters to leave melee
 * range between the initial hit & the shadow mimic.
 *
 * XXX: refactor this with attack/fight code!
 *
 * @param target    The creature to be struck.
 * @return          Whether the player is melee range of the target, using
 *                  their current weapon.
 */
static bool _in_melee_range(actor* target)
{
    const int dist = (you.pos() - target->pos()).abs();
    return dist < 2 || (dist <= 2 && you.reach_range() != REACH_NONE);
}

void dithmenos_shadow_melee(actor* target)
{
    if (!target
        || !target->alive()
        || !_in_melee_range(target)
        || !_dithmenos_shadow_acts())
    {
        return;
    }

    monster* mon = shadow_monster();
    if (!mon)
        return;

    mon->target     = target->pos();
    mon->foe        = target->mindex();

    fight_melee(mon, target);

    shadow_monster_reset(mon);
}

void dithmenos_shadow_throw(coord_def target, const item_def &item)
{
    if (target.origin()
        || !_dithmenos_shadow_acts())
    {
        return;
    }

    monster* mon = shadow_monster();
    if (!mon)
        return;

    int ammo_index = get_mitm_slot(10);
    if (ammo_index != NON_ITEM)
    {
        item_def& new_item = mitm[ammo_index];
        new_item.base_type = item.base_type;
        new_item.sub_type  = item.sub_type;
        new_item.quantity  = 1;
        new_item.rnd = 1;
        new_item.flags    |= ISFLAG_SUMMONED;
        mon->inv[MSLOT_MISSILE] = ammo_index;

        mon->target = target;

        bolt beem;
        beem.target = target;
        setup_monster_throw_beam(mon, beem);
        beem.item = &mitm[mon->inv[MSLOT_MISSILE]];
        mons_throw(mon, beem, mon->inv[MSLOT_MISSILE]);
    }

    shadow_monster_reset(mon);
}

void dithmenos_shadow_spell(bolt* orig_beam, spell_type spell)
{
    if (!orig_beam
        || orig_beam->target.origin()
        || (orig_beam->is_enchantment() && !is_valid_mon_spell(spell))
        || !_dithmenos_shadow_acts())
    {
        return;
    }

    const coord_def target = orig_beam->target;

    monster* mon = shadow_monster();
    if (!mon)
        return;

    // Don't let shadow spells get too powerful.
    mon->set_hit_dice(max(1,
                          min(3 * spell_difficulty(spell),
                              you.experience_level) / 2));

    mon->target = target;
    if (actor_at(target))
        mon->foe = actor_at(target)->mindex();

    spell_type shadow_spell = spell;
    if (!orig_beam->is_enchantment())
    {
        shadow_spell = (orig_beam->pierce) ? SPELL_SHADOW_BOLT
                                           : SPELL_SHADOW_SHARD;
    }

    bolt beem;
    beem.target = target;
    mprf(MSGCH_FRIEND_SPELL, "%s mimicks your spell!",
         mon->name(DESC_THE).c_str());
    mons_cast(mon, beem, shadow_spell, MON_SPELL_WIZARD, false);

    shadow_monster_reset(mon);
}

static potion_type _gozag_potion_list[][4] =
{
    { POT_HEAL_WOUNDS, NUM_POTIONS, NUM_POTIONS, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_CURING, NUM_POTIONS, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_MAGIC, NUM_POTIONS, NUM_POTIONS, },
    { POT_CURING, POT_MAGIC, NUM_POTIONS, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_BERSERK_RAGE, NUM_POTIONS, NUM_POTIONS },
    { POT_HASTE, POT_HEAL_WOUNDS, NUM_POTIONS, NUM_POTIONS },
    { POT_HASTE, POT_BRILLIANCE, NUM_POTIONS, NUM_POTIONS },
    { POT_HASTE, POT_AGILITY, NUM_POTIONS, NUM_POTIONS },
    { POT_MIGHT, POT_AGILITY, NUM_POTIONS, NUM_POTIONS },
    { POT_HASTE, POT_FLIGHT, NUM_POTIONS, NUM_POTIONS },
    { POT_HASTE, POT_RESISTANCE, NUM_POTIONS, NUM_POTIONS },
    { POT_RESISTANCE, POT_AGILITY, NUM_POTIONS, NUM_POTIONS },
    { POT_RESISTANCE, POT_FLIGHT, NUM_POTIONS, NUM_POTIONS },
    { POT_INVISIBILITY, POT_AGILITY, NUM_POTIONS , NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_CURING, POT_MAGIC, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_CURING, POT_BERSERK_RAGE, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_HASTE, POT_AGILITY, NUM_POTIONS },
    { POT_MIGHT, POT_AGILITY, POT_BRILLIANCE, NUM_POTIONS },
    { POT_HASTE, POT_AGILITY, POT_FLIGHT, NUM_POTIONS },
    { POT_FLIGHT, POT_AGILITY, POT_INVISIBILITY, NUM_POTIONS },
    { POT_RESISTANCE, POT_MIGHT, POT_AGILITY, NUM_POTIONS },
    { POT_RESISTANCE, POT_MIGHT, POT_HASTE, NUM_POTIONS },
    { POT_RESISTANCE, POT_INVISIBILITY, POT_AGILITY, NUM_POTIONS },
};

static void _gozag_add_potions(CrawlVector &vec, potion_type *which)
{
    for (; *which != NUM_POTIONS; which++)
    {
        bool dup = false;
        for (unsigned int i = 0; i < vec.size(); i++)
            if (vec[i].get_int() == *which)
            {
                dup = true;
                break;
            }
        if (!dup)
            vec.push_back(*which);
    }
}

#define ADD_POTIONS(a,b) _gozag_add_potions(a, b[random2(ARRAYSZ(b))])

static int _gozag_faith_adjusted_price(int price)
{
    return price - (you.faith() * price)/3;
}

int gozag_potion_price()
{
    int multiplier = GOZAG_POTION_BASE_MULTIPLIER
                     + you.attribute[ATTR_GOZAG_POTIONS];
    int price = multiplier * 15; // arbitrary
    return _gozag_faith_adjusted_price(price);
}

bool gozag_setup_potion_petition(bool quiet)
{
    const int gold_min = gozag_potion_price();
    if (you.gold < gold_min)
    {
        if (!quiet)
        {
            mprf("You need at least %d gold to purchase potions right now!",
                 gold_min);
        }
        return false;
    }

    return true;
}

bool gozag_potion_petition()
{
    CrawlVector *pots[GOZAG_MAX_POTIONS];
    int prices[GOZAG_MAX_POTIONS];

    item_def dummy;
    dummy.base_type = OBJ_POTIONS;
    dummy.quantity = 1;

    if (!you.props.exists(make_stringf(GOZAG_POTIONS_KEY, 0)))
    {
        bool affordable_potions = false;
        while (!affordable_potions)
        {
            for (int i = 0; i < GOZAG_MAX_POTIONS; i++)
            {
                prices[i] = 0;
                int multiplier = GOZAG_POTION_BASE_MULTIPLIER
                                 + you.attribute[ATTR_GOZAG_POTIONS];
                string key = make_stringf(GOZAG_POTIONS_KEY, i);
                you.props.erase(key);
                you.props[key].new_vector(SV_INT, SFLAG_CONST_TYPE);
                pots[i] = &you.props[key].get_vector();

                ADD_POTIONS(*pots[i], _gozag_potion_list);
                if (coinflip())
                    ADD_POTIONS(*pots[i], _gozag_potion_list);

                for (int j = 0; j < pots[i]->size(); j++)
                {
                    dummy.sub_type = (*pots[i])[j].get_int();
                    prices[i] += item_value(dummy, true);
                    dprf("%d", item_value(dummy, true));
                }
                dprf("pre: %d", prices[i]);
                prices[i] *= multiplier;
                dprf("mid: %d", prices[i]);
                prices[i] /= 10;
                dprf("post: %d", prices[i]);
                key = make_stringf(GOZAG_PRICE_KEY, i);
                you.props[key].get_int() = prices[i];

                if (prices[i] <= gozag_potion_price())
                    affordable_potions = true;
            }
        }
    }
    else
    {
        for (int i = 0; i < GOZAG_MAX_POTIONS; i++)
        {
            string key = make_stringf(GOZAG_POTIONS_KEY, i);
            pots[i] = &you.props[key].get_vector();
            key = make_stringf(GOZAG_PRICE_KEY, i);
            prices[i] = you.props[key].get_int();
        }
    }

    int keyin = 0;
    int faith_price = 0;

    while (true)
    {
        if (crawl_state.seen_hups)
            return false;

        clear_messages();
        for (int i = 0; i < GOZAG_MAX_POTIONS; i++)
        {
            faith_price = _gozag_faith_adjusted_price(prices[i]);
            string line = make_stringf("  [%c] - %d gold - ", i + 'a',
                                       faith_price);
            vector<string> pot_names;
            for (int j = 0; j < pots[i]->size(); j++)
                pot_names.push_back(potion_type_name((*pots[i])[j].get_int()));
            line += comma_separated_line(pot_names.begin(), pot_names.end());
            mpr_nojoin(MSGCH_PLAIN, line.c_str());
        }
        mprf(MSGCH_PROMPT, "Purchase which effect?");
        keyin = toalower(get_ch()) - 'a';
        if (keyin < 0 || keyin > GOZAG_MAX_POTIONS - 1)
            continue;

        faith_price = _gozag_faith_adjusted_price(prices[keyin]);
        if (you.gold < faith_price)
        {
            mpr("You don't have enough gold for that!");
            more();
            continue;
        }

        break;
    }

    ASSERT(you.gold >= faith_price);
    you.del_gold(faith_price);
    you.attribute[ATTR_GOZAG_GOLD_USED] += faith_price;
    for (int j = 0; j < pots[keyin]->size(); j++)
    {
        potion_effect(static_cast<potion_type>((*pots[keyin])[j].get_int()),
                      40);
    }

    you.attribute[ATTR_GOZAG_POTIONS]++;

    for (int i = 0; i < GOZAG_MAX_POTIONS; i++)
    {
        string key = make_stringf(GOZAG_POTIONS_KEY, i);
        you.props.erase(key);
        key = make_stringf(GOZAG_PRICE_KEY, i);
        you.props.erase(key);
    }

    return true;
}

/**
 * How many shop types are offered with each use of Call Merchant?
 */
static int _gozag_max_shops()
{
    const int max_non_food_shops = 3;

    // add a food shop if you can eat (non-mu/dj)
    if (!you_foodless_normally())
        return max_non_food_shops + 1;
    return max_non_food_shops;
}

/**
 * The price to order a merchant from Gozag. Doesn't depend on the shop's
 * type or contents. The maximum possible price is used as the minimum amount
 * of gold you need to use the ability.
 */
int gozag_price_for_shop(bool max)
{
    // This value probably needs tweaking.
    const int base = max ? 1000 : random_range(500, 1000);
    const int price = base
                      * (GOZAG_SHOP_BASE_MULTIPLIER
                         + GOZAG_SHOP_MOD_MULTIPLIER
                           * you.attribute[ATTR_GOZAG_SHOPS])
                      / GOZAG_SHOP_BASE_MULTIPLIER;
    return max ? _gozag_faith_adjusted_price(price) : price;
}

static vector<level_id> _get_gozag_shop_candidates(int *max_absdepth)
{
    vector<level_id> candidates;

    branch_type allowed_branches[3] = {BRANCH_DUNGEON, BRANCH_VAULTS, BRANCH_DEPTHS};
    for (int ii = 0; ii < 3; ii++)
    {
        branch_type i = allowed_branches[ii];

        level_id lid(i, brdepth[i]);

        // Base shop level number on the deepest we can place a shop
        // in the given game; this is constant for as long as we can place
        // shops and is intended to prevent scummy behaviour.
        if (max_absdepth)
        {
            const int absdepth = branches[i].absdepth + brdepth[i] - 1;
            if (absdepth > *max_absdepth)
                *max_absdepth = absdepth;
        }

        for (int j = 1; j <= brdepth[i] && candidates.size() < 4; j++)
        {
            // Don't try to place shops on Vaults:$, since they'll be totally
            // insignificant next to the regular loot/shops
            if (i == BRANCH_VAULTS && j == brdepth[i])
                continue;
            lid.depth = j;
            if (!is_existing_level(lid)
                && !you.props.exists(make_stringf(GOZAG_SHOP_KEY,
                                                  lid.describe().c_str())))
            {
                candidates.push_back(lid);
            }
        }
    }

    return candidates;
}

bool gozag_setup_call_merchant(bool quiet)
{
    const int gold_min = gozag_price_for_shop(true);
    if (you.gold < gold_min)
    {
        if (!quiet)
        {
            mprf("You currently need %d gold to open negotiations with a "
                 "merchant.", gold_min);
        }
        return false;
    }

    int max_absdepth = 0;
    vector<level_id> candidates = _get_gozag_shop_candidates(&max_absdepth);

    if (!candidates.size())
    {
        const map_def *shop_vault = find_map_by_name("serial_shops"); // XXX
        if (!shop_vault)
            die("Someone changed the shop vault name!");
        if (!shop_vault->depths.is_usable_in(level_id::current()))
        {
            if (!quiet)
            {
                mprf("No merchants are willing to come to this level in the "
                     "absence of new frontiers.");
                return false;
            }
        }
        if (grd(you.pos()) != DNGN_FLOOR)
        {
            if (!quiet)
            {
                mprf("You need to be standing on an open floor tile to call a "
                     "shop here.");
                return false;
            }
        }
    }

    return true;
}

/**
 * Is the given index within the valid range for gozag shop offers?
 */
static bool _gozag_valid_shop_index(int index)
{
    return index >= 0 && index < _gozag_max_shops();
}

/**
 * What is the type of shop that gozag is offering at the given index?
 */
static shop_type _gozag_shop_type(int index)
{
    ASSERT(_gozag_valid_shop_index(index));
    const int type =
        you.props[make_stringf(GOZAG_SHOP_TYPE_KEY, index)].get_int();
    return static_cast<shop_type>(type);
}

/**
 * What is the price of calling the shop that gozag is offering at the given
 * index, including the effects of faith?
 */
static int _gozag_shop_price(int index)
{
    ASSERT(_gozag_valid_shop_index(index));
    // the base cost
    const int cost = you.props[make_stringf(GOZAG_SHOP_COST_KEY,
                                            index)].get_int();
    // shop cost adjusted for faith (amulet, mutation)
    return _gozag_faith_adjusted_price(cost);
}

/**
 * Does the given shop type duplicate an earlier-chosen one?
 *
 * @param cur       The index of the current shop.
 *                  (Assumption: shops [0,cur-1] have been chosen already.)
 * @param type      The type of shop being considered.
 * @return          Whether the given shop type duplicates a predecessor.
 */
static bool _duplicate_shop_type(int cur, shop_type type)
{
    for (int i = 0; i < cur; i++)
        if (_gozag_shop_type(i) == type)
            return true;

    return false;
}

/**
 * Initialize the set of shops currently offered to the player through Call
 * Merchant.
 *
 * @param index     The index of the shop offer to be defined.
 */
static void _setup_gozag_shop(int index)
{
    ASSERT(!you.props.exists(make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, index)));

    shop_type type = NUM_SHOPS;
    if (index == 0 && !you_foodless_normally())
        type = SHOP_FOOD;
    else
    {
        do
        {
            type = static_cast<shop_type>(random2(NUM_SHOPS));
        }
        while (_duplicate_shop_type(index, type));
    }
    you.props[make_stringf(GOZAG_SHOP_TYPE_KEY, index)].get_int() = type;

    you.props[make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, index)].get_string()
                                    = make_name(random_int(), false);

    const bool need_suffix = type != SHOP_GENERAL
                             && type != SHOP_GENERAL_ANTIQUE
                             && type != SHOP_DISTILLERY;
    you.props[make_stringf(GOZAG_SHOP_SUFFIX_KEY, index)].get_string()
                                    = need_suffix ?
                                      random_choose("Shoppe", "Boutique",
                                                    "Emporium", "Shop", NULL) :
                                      "";

    you.props[make_stringf(GOZAG_SHOP_COST_KEY, index)].get_int()
                                    = gozag_price_for_shop();
}

/**
 * Build a string describing the name, price & type of the shop being offered
 * at the given index.
 *
 * @param index     The index of the shop to be described.
 * @return          The shop description.
 *                  E.g. "[a]   973 gold - Cranius' Magic Scroll Boutique"
 */
static string _describe_gozag_shop(int index)
{
    const int cost = _gozag_shop_price(index);

    const char offer_letter = 'a' + index;
    const string shop_name =
        apostrophise(you.props[make_stringf(GOZAG_SHOPKEEPER_NAME_KEY,
                                            index)].get_string());
    const shop_type type = _gozag_shop_type(index);
    const string suffix =
        you.props[make_stringf(GOZAG_SHOP_SUFFIX_KEY, index)].get_string();

    return make_stringf("  [%c] %5d gold - %s %s %s",
                        offer_letter,
                        cost,
                        shop_name.c_str(),
                        shop_type_name(type).c_str(),
                        suffix.c_str());
}

/**
 * Let the player choose from the currently available merchants to call.
 *
 * @param   The index of the chosen shop; -1 if none was chosen (due to e.g.
 *          a seen_hup).
 */
static int _gozag_choose_shop()
{
    if (crawl_state.seen_hups)
        return -1;

    clear_messages();
    for (int i = 0; i < _gozag_max_shops(); i++)
        mpr_nojoin(MSGCH_PLAIN, _describe_gozag_shop(i).c_str());

    mprf(MSGCH_PROMPT, "Fund which merchant?");
    const int shop_index = toalower(get_ch()) - 'a';
    if (shop_index < 0 || shop_index > _gozag_max_shops() - 1)
        return _gozag_choose_shop(); // tail recurse

    if (you.gold < _gozag_shop_price(shop_index))
    {
        mpr("You don't have enough gold to fund that merchant!");
        more();
        return _gozag_choose_shop(); // tail recurse
    }

    return shop_index;
}

/**
 * Make a vault spec for the gozag shop offer at the given index.
 */
static string _gozag_shop_spec(int index)
{
    const shop_type type = _gozag_shop_type(index);
    const string name =
        you.props[make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, index)];

    string suffix = replace_all(
                                you.props[make_stringf(GOZAG_SHOP_SUFFIX_KEY,
                                                       index)]
                                .get_string(), " ", "_");
    if (!suffix.empty())
        suffix = " suffix:" + suffix;

    return make_stringf("%s shop name:%s%s gozag",
                        shoptype_to_str(type),
                        replace_all(name, " ", "_").c_str(),
                        suffix.c_str());

}

/**
 * "Place" a gozag-called shop in a level you haven't yet seen.
 * (Mark it to be placed when the level is generated & message/annotate
 * appropriately.)
 *
 * @param index         The index of the shop offer to be placed.
 * @param candidates    Potentially viable levels to place the shop in.
 */
static void _gozag_place_shop_offlevel(int index, vector<level_id> &candidates)
{
    ASSERT(candidates.size());

    vector<int> weights;
    for (unsigned int j = 0; j < candidates.size(); j++)
        weights.push_back(candidates.size() - j);
    const int which = choose_random_weighted(weights.begin(), weights.end());
    const level_id lid = candidates[which];

    // "place" the shop
    you.props[make_stringf(GOZAG_SHOP_KEY, lid.describe().c_str())]
                                    .get_string() = _gozag_shop_spec(index);

    const string name =
        you.props[make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, index)];
    mprf(MSGCH_GOD, "%s sets up shop in %s.", name.c_str(),
         branches[lid.branch].longname);
    dprf("%s", lid.describe().c_str());

    mark_offlevel_shop(lid, _gozag_shop_type(index));
}

/**
 * Place a gozag-called shop in your current location.
 *
 * @param index     The index of the shop offer to be placed.
 */
static void _gozag_place_shop_here(int index)
{
    ASSERT(grd(you.pos()) == DNGN_FLOOR);
    keyed_mapspec kmspec;
    kmspec.set_feat(_gozag_shop_spec(index), false);
    if (!kmspec.get_feat().shop.get())
        die("Invalid shop spec?");

    feature_spec feat = kmspec.get_feat();
    shop_spec *spec = feat.shop.get();
    ASSERT(spec);
    place_spec_shop(you.pos(), *spec);

    link_items();
    env.markers.add(new map_feature_marker(you.pos(),
                                           DNGN_ABANDONED_SHOP));
    env.markers.clear_need_activate();

    mprf(MSGCH_GOD, "A shop appears before you!");
}

/**
 * Attempt to call the shop specified at the given index.
 *
 * @param index     The index of the shop (in gozag props)
 */
static void _gozag_place_shop(int index)
{
    vector<level_id> candidates = _get_gozag_shop_candidates(NULL);

    if (candidates.size())
        _gozag_place_shop_offlevel(index, candidates);
    else
        _gozag_place_shop_here(index);
}

bool gozag_call_merchant()
{
    // Set up some dummy shops.
    // Generate some shop inventory and store it as a store spec.
    // We still set up the shops in advance in case of hups.
    for (int i = 0; i < _gozag_max_shops(); i++)
        if (!you.props.exists(make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, i)))
            _setup_gozag_shop(i);

    const int shop_index = _gozag_choose_shop();
    if (shop_index == -1) // hup!
        return false;

    ASSERT(shop_index >= 0 && shop_index < _gozag_max_shops());

    const int cost = _gozag_shop_price(shop_index);
    ASSERT(you.gold >= cost);

    you.del_gold(cost);
    you.attribute[ATTR_GOZAG_GOLD_USED] += cost;

    _gozag_place_shop(shop_index);

    you.attribute[ATTR_GOZAG_SHOPS]++;
    you.attribute[ATTR_GOZAG_SHOPS_CURRENT]++;

    for (int j = 0; j < _gozag_max_shops(); j++)
    {
        you.props.erase(make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, j));
        you.props.erase(make_stringf(GOZAG_SHOP_TYPE_KEY, j));
        you.props.erase(make_stringf(GOZAG_SHOP_SUFFIX_KEY, j));
        you.props.erase(make_stringf(GOZAG_SHOP_COST_KEY, j));
    }

    return true;
}

typedef struct
{
    monster_type type;
    branch_type  branch;
    int          susceptibility;
} bribability;

bribability mons_bribability[] =
{
    // Orcs
    { MONS_ORC,             BRANCH_ORC, 1 },
    { MONS_ORC_PRIEST,      BRANCH_ORC, 2 },
    { MONS_ORC_WIZARD,      BRANCH_ORC, 2 },
    { MONS_ORC_WARRIOR,     BRANCH_ORC, 3 },
    { MONS_ORC_KNIGHT,      BRANCH_ORC, 4 },
    { MONS_ORC_SORCERER,    BRANCH_ORC, 4 },
    { MONS_ORC_HIGH_PRIEST, BRANCH_ORC, 4 },
    { MONS_ORC_WARLORD,     BRANCH_ORC, 5 },

    // Elves
    { MONS_DEEP_ELF_FIGHTER,       BRANCH_ELF, 1 },
    { MONS_DEEP_ELF_MAGE,          BRANCH_ELF, 1 },
    { MONS_DEEP_ELF_SUMMONER,      BRANCH_ELF, 2 },
    { MONS_DEEP_ELF_CONJURER,      BRANCH_ELF, 2 },
    { MONS_DEEP_ELF_PRIEST,        BRANCH_ELF, 2 },
    { MONS_DEEP_ELF_KNIGHT,        BRANCH_ELF, 3 },
    { MONS_DEEP_ELF_HIGH_PRIEST,   BRANCH_ELF, 4 },
    { MONS_DEEP_ELF_DEMONOLOGIST,  BRANCH_ELF, 4 },
    { MONS_DEEP_ELF_ANNIHILATOR,   BRANCH_ELF, 4 },
    { MONS_DEEP_ELF_SORCERER,      BRANCH_ELF, 4 },
    { MONS_DEEP_ELF_DEATH_MAGE,    BRANCH_ELF, 4 },
    { MONS_DEEP_ELF_BLADEMASTER,   BRANCH_ELF, 4 },
    { MONS_DEEP_ELF_MASTER_ARCHER, BRANCH_ELF, 4 },

    // Nagas and salamanders
    { MONS_NAGA,                 BRANCH_SNAKE, 1 },
    { MONS_SALAMANDER,           BRANCH_SNAKE, 1 },
    { MONS_NAGA_MAGE,            BRANCH_SNAKE, 2 },
    { MONS_NAGA_SHARPSHOOTER,    BRANCH_SNAKE, 2 },
    { MONS_NAGA_RITUALIST,       BRANCH_SNAKE, 2 },
    { MONS_SALAMANDER_MYSTIC,    BRANCH_SNAKE, 2 },
    { MONS_NAGA_WARRIOR,         BRANCH_SNAKE, 3 },
    { MONS_GREATER_NAGA,         BRANCH_SNAKE, 4 },
    { MONS_SALAMANDER_FIREBRAND, BRANCH_SNAKE, 4 },

    { MONS_MERFOLK,            BRANCH_SHOALS, 1 },
    { MONS_SIREN,              BRANCH_SHOALS, 1 },
    { MONS_MERFOLK_AVATAR,     BRANCH_SHOALS, 2 },
    { MONS_MERFOLK_IMPALER,    BRANCH_SHOALS, 3 },
    { MONS_MERFOLK_JAVELINEER, BRANCH_SHOALS, 3 },
    { MONS_MERFOLK_AQUAMANCER, BRANCH_SHOALS, 4 },

    // Humans
    { MONS_HUMAN,               BRANCH_VAULTS, 1 },
    { MONS_WIZARD,              BRANCH_VAULTS, 2 },
    { MONS_NECROMANCER,         BRANCH_VAULTS, 2 },
    { MONS_HELL_KNIGHT,         BRANCH_VAULTS, 3 },
    { MONS_VAULT_GUARD,         BRANCH_VAULTS, 3 },
    { MONS_VAULT_SENTINEL,      BRANCH_VAULTS, 3 },
    { MONS_IRONBRAND_CONVOKER,  BRANCH_VAULTS, 3 },
    { MONS_IRONHEART_PRESERVER, BRANCH_VAULTS, 3 },
    { MONS_VAULT_WARDEN,        BRANCH_VAULTS, 4 },

    // Draconians
    { MONS_DRACONIAN,             BRANCH_ZOT, 1 },
    { MONS_BLACK_DRACONIAN,       BRANCH_ZOT, 2 },
    { MONS_MOTTLED_DRACONIAN,     BRANCH_ZOT, 2 },
    { MONS_YELLOW_DRACONIAN,      BRANCH_ZOT, 2 },
    { MONS_GREEN_DRACONIAN,       BRANCH_ZOT, 2 },
    { MONS_PURPLE_DRACONIAN,      BRANCH_ZOT, 2 },
    { MONS_RED_DRACONIAN,         BRANCH_ZOT, 2 },
    { MONS_WHITE_DRACONIAN,       BRANCH_ZOT, 2 },
    { MONS_GREY_DRACONIAN,        BRANCH_ZOT, 2 },
    { MONS_PALE_DRACONIAN,        BRANCH_ZOT, 2 },
    { MONS_DRACONIAN_CALLER,      BRANCH_ZOT, 3 },
    { MONS_DRACONIAN_MONK,        BRANCH_ZOT, 3 },
    { MONS_DRACONIAN_ZEALOT,      BRANCH_ZOT, 3 },
    { MONS_DRACONIAN_SHIFTER,     BRANCH_ZOT, 3 },
    { MONS_DRACONIAN_ANNIHILATOR, BRANCH_ZOT, 3 },
    { MONS_DRACONIAN_KNIGHT,      BRANCH_ZOT, 3 },
    { MONS_DRACONIAN_SCORCHER,    BRANCH_ZOT, 3 },

    // Demons
    { MONS_IRON_IMP,        BRANCH_DIS, 1 },
    { MONS_RUST_DEVIL,      BRANCH_DIS, 2 },
    { MONS_HELL_SENTINEL,   BRANCH_DIS, 5 },
    { MONS_CRIMSON_IMP,     BRANCH_GEHENNA, 1 },
    { MONS_RED_DEVIL,       BRANCH_GEHENNA, 2 },
    { MONS_ORANGE_DEMON,    BRANCH_GEHENNA, 2 },
    { MONS_SMOKE_DEMON,     BRANCH_GEHENNA, 3 },
    { MONS_SUN_DEMON,       BRANCH_GEHENNA, 3 },
    { MONS_HELLION,         BRANCH_GEHENNA, 4 },
    { MONS_BALRUG,          BRANCH_GEHENNA, 4 },
    { MONS_BRIMSTONE_FIEND, BRANCH_GEHENNA, 5 },
    { MONS_WHITE_IMP,       BRANCH_COCYTUS, 1 },
    { MONS_BLUE_DEVIL,      BRANCH_COCYTUS, 2 },
    { MONS_ICE_DEVIL,       BRANCH_COCYTUS, 3 },
    { MONS_BLIZZARD_DEMON,  BRANCH_COCYTUS, 4 },
    { MONS_ICE_FIEND,       BRANCH_COCYTUS, 5 },
    { MONS_SHADOW_IMP,      BRANCH_TARTARUS, 1 },
    { MONS_HELLWING,        BRANCH_TARTARUS, 2 },
    { MONS_SOUL_EATER,      BRANCH_TARTARUS, 3 },
    { MONS_REAPER,          BRANCH_TARTARUS, 4 },
    { MONS_SHADOW_DEMON,    BRANCH_TARTARUS, 4 },
    { MONS_SHADOW_FIEND,    BRANCH_TARTARUS, 5 },
};

// An x-in-16 chance of a monster of the given type being bribed.
// Tougher monsters have a stronger chance of being bribed.
int gozag_type_bribable(monster_type type, bool force)
{
    if (!you_worship(GOD_GOZAG) && !force)
        return 0;

    for (unsigned int i = 0; i < ARRAYSZ(mons_bribability); i++)
    {
        if (mons_bribability[i].type == type)
        {
            return force || branch_bribe[mons_bribability[i].branch]
                   ? mons_bribability[i].susceptibility
                   : 0;
        }
    }

    return 0;
}

branch_type gozag_bribable_branch(monster_type type)
{

    for (unsigned int i = 0; i < ARRAYSZ(mons_bribability); i++)
    {
        if (mons_bribability[i].type == type)
            return mons_bribability[i].branch;
    }

    return NUM_BRANCHES;
}

bool gozag_branch_bribable(branch_type branch)
{
    for (unsigned int i = 0; i < ARRAYSZ(mons_bribability); i++)
    {
        if (mons_bribability[i].branch == branch)
            return true;
    }

    return false;
}

int gozag_branch_bribe_susceptibility(branch_type branch)
{
    int susceptibility = 0;
    for (unsigned int i = 0; i < ARRAYSZ(mons_bribability); i++)
    {
        if (mons_bribability[i].branch == branch
            && susceptibility < mons_bribability[i].susceptibility)
        {
            susceptibility = mons_bribability[i].susceptibility;
        }
    }

    return susceptibility;
}

void gozag_deduct_bribe(branch_type br, int amount)
{
    if (branch_bribe[br] <= 0)
        return;

    branch_bribe[br] = max(0, branch_bribe[br] - amount);
    if (branch_bribe[br] <= 0)
    {
        mprf(MSGCH_DURATION, "Your bribe of %s has been exhausted.",
             branches[br].longname);
        add_daction(DACT_BRIBE_TIMEOUT);
    }
}

bool gozag_check_bribe_branch(bool quiet)
{
    const int bribe_amount = GOZAG_BRIBE_AMOUNT;
    if (you.gold < bribe_amount)
    {
        if (!quiet)
            mprf("You need at least %d gold to offer a bribe.", bribe_amount);
        return false;
    }
    branch_type branch = you.where_are_you;
    branch_type branch2 = NUM_BRANCHES;
    if (feat_is_branch_entrance(grd(you.pos())))
    {
        for (branch_iterator it; it; ++it)
            if (it->entry_stairs == grd(you.pos())
                && gozag_branch_bribable(it->id))
            {
                branch2 = it->id;
                break;
            }
    }
    const string who = make_stringf("the denizens of %s",
                                   branches[branch].longname);
    const string who2 = branch2 != NUM_BRANCHES
                        ? make_stringf("the denizens of %s",
                                       branches[branch2].longname)
                        : "";
    if (!gozag_branch_bribable(branch)
        && (branch2 == NUM_BRANCHES
            || !gozag_branch_bribable(branch2)))
    {
        if (!quiet)
        {
            if (branch2 != NUM_BRANCHES)
                mprf("You can't bribe %s or %s.", who.c_str(), who2.c_str());
            else
                mprf("You can't bribe %s.", who.c_str());
        }
        return false;
    }
    return true;
}

bool gozag_bribe_branch()
{
    const int bribe_amount = GOZAG_BRIBE_AMOUNT;
    ASSERT(you.gold >= bribe_amount);
    bool prompted = false;
    branch_type branch = you.where_are_you;
    if (feat_is_branch_entrance(grd(you.pos())))
    {
        for (branch_iterator it; it; ++it)
            if (it->entry_stairs == grd(you.pos())
                && gozag_branch_bribable(it->id))
            {
                string prompt =
                    make_stringf("Do you want to bribe the denizens of %s?",
                                 it->longname);
                if (yesno(prompt.c_str(), true, 'n'))
                {
                    branch = it->id;
                    prompted = true;
                }
                break;
            }
    }
    string who = make_stringf("the denizens of %s",
                              branches[branch].longname);
    if (!gozag_branch_bribable(branch))
    {
        mprf("You can't bribe %s.", who.c_str());
        return false;
    }

    string prompt =
        make_stringf("Do you want to bribe the denizens of %s?",
                     branches[branch].longname);

    if (prompted || yesno(prompt.c_str(), true, 'n'))
    {
        you.del_gold(bribe_amount);
        you.attribute[ATTR_GOZAG_GOLD_USED] += bribe_amount;
        branch_bribe[branch] += bribe_amount;
        string msg = make_stringf(" spreads your bribe to %s!",
                                  branches[branch].longname);
        simple_god_message(msg.c_str());
        add_daction(DACT_SET_BRIBES);
        return true;
    }

    canned_msg(MSG_OK);
    return false;
}

spret_type qazlal_upheaval(coord_def target, bool quiet, bool fail)
{
    int pow = you.skill(SK_INVOCATIONS, 6);
    const int max_radius = pow >= 100 ? 2 : 1;

    bolt beam;
    beam.name        = "****";
    beam.source_id   = MID_PLAYER;
    beam.source_name = "you";
    beam.thrower     = KILL_YOU;
    beam.range       = LOS_RADIUS;
    beam.damage      = calc_dice(3, 27 + div_rand_round(2 * pow, 5));
    beam.hit         = AUTOMATIC_HIT;
    beam.glyph       = dchar_glyph(DCHAR_EXPLOSION);
    beam.loudness    = 10;
#ifdef USE_TILE
    beam.tile_beam = -1;
#endif
    beam.draw_delay  = 0;

    if (target.origin())
    {
        dist spd;
        targetter_smite tgt(&you, LOS_RADIUS, 0, max_radius);
        if (!spell_direction(spd, beam, DIR_TARGET, TARG_HOSTILE,
                             LOS_RADIUS, false, true, false, NULL,
                             "Aiming: <white>Upheaval</white>", true,
                             &tgt))
        {
            return SPRET_ABORT;
        }
        bolt tempbeam;
        tempbeam.source    = beam.target;
        tempbeam.target    = beam.target;
        tempbeam.flavour   = BEAM_MISSILE;
        tempbeam.ex_size   = max_radius;
        tempbeam.hit       = AUTOMATIC_HIT;
        tempbeam.damage    = dice_def(AUTOMATIC_HIT, 1);
        tempbeam.thrower   = KILL_YOU;
        tempbeam.is_tracer = true;
        tempbeam.explode(false);
        if (tempbeam.beam_cancelled)
            return SPRET_ABORT;
    }
    else
        beam.target = target;

    fail_check();

    string message = "";

    switch (random2(4))
    {
        case 0:
            beam.name     = "blast of magma";
            beam.flavour  = BEAM_LAVA;
            beam.colour   = RED;
            beam.hit_verb = "engulfs";
            message       = "Magma suddenly erupts from the ground!";
            break;
        case 1:
            beam.name    = "blast of ice";
            beam.flavour = BEAM_ICE;
            beam.colour  = WHITE;
            message      = "A blizzard blasts the area with ice!";
            break;
        case 2:
            beam.name    = "cutting wind";
            beam.flavour = BEAM_AIR;
            beam.colour  = LIGHTGRAY;
            message      = "A storm cloud blasts the area with cutting wind!";
            break;
        case 3:
            beam.name    = "blast of rubble";
            beam.flavour = BEAM_FRAG;
            beam.colour  = BROWN;
            message      = "The ground shakes violently, spewing rubble!";
            break;
        default:
            break;
    }

    vector<coord_def> affected;
    affected.push_back(beam.target);
    for (radius_iterator ri(beam.target, max_radius, C_ROUND, LOS_SOLID, true);
         ri; ++ri)
    {
        if (!in_bounds(*ri) || cell_is_solid(*ri))
            continue;

        int chance = pow;

        bool adj = adjacent(beam.target, *ri);
        if (!adj && max_radius > 1)
            chance -= 100;
        if (adj && max_radius > 1 || x_chance_in_y(chance, 100))
        {
            if (beam.flavour == BEAM_FRAG || !cell_is_solid(*ri))
                affected.push_back(*ri);
        }
    }

    for (unsigned int i = 0; i < affected.size(); i++)
    {
        beam.draw(affected[i]);
        if (!quiet)
            scaled_delay(25);
    }
    if (!quiet)
    {
        scaled_delay(100);
        mprf(MSGCH_GOD, "%s", message.c_str());
    }
    else
        scaled_delay(25);

    int wall_count = 0;

    for (unsigned int i = 0; i < affected.size(); i++)
    {
        coord_def pos = affected[i];
        beam.source = pos;
        beam.target = pos;
        beam.fire();

        switch (beam.flavour)
        {
            case BEAM_LAVA:
                if (grd(pos) == DNGN_FLOOR && !actor_at(pos) && coinflip())
                {
                    temp_change_terrain(
                        pos, DNGN_LAVA,
                        random2(you.skill(SK_INVOCATIONS, BASELINE_DELAY)),
                        TERRAIN_CHANGE_FLOOD);
                }
                break;
            case BEAM_AIR:
                if (!cell_is_solid(pos) && env.cgrid(pos) == EMPTY_CLOUD
                    && coinflip())
                {
                    place_cloud(CLOUD_STORM, pos,
                                random2(you.skill_rdiv(SK_INVOCATIONS, 1, 4)),
                                &you);
                }
                break;
            case BEAM_FRAG:
                if (((grd(pos) == DNGN_ROCK_WALL
                     || grd(pos) == DNGN_CLEAR_ROCK_WALL
                     || grd(pos) == DNGN_SLIMY_WALL)
                     && x_chance_in_y(pow / 4, 100)
                    || grd(pos) == DNGN_CLOSED_DOOR
                    || grd(pos) == DNGN_RUNED_DOOR
                    || grd(pos) == DNGN_OPEN_DOOR
                    || grd(pos) == DNGN_SEALED_DOOR
                    || grd(pos) == DNGN_GRATE))
                {
                    noisy(30, pos);
                    destroy_wall(pos);
                    wall_count++;
                }
                break;
            default:
                break;
        }
    }

    if (wall_count && !quiet)
        mpr("Ka-crash!");

    return SPRET_SUCCESS;
}

void qazlal_elemental_force()
{
    vector<coord_def> targets;
    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_ROUND, true); ri; ++ri)
    {
        if (env.cgrid(*ri) != EMPTY_CLOUD)
        {
            switch (env.cloud[env.cgrid(*ri)].type)
            {
            case CLOUD_FIRE:
            case CLOUD_COLD:
            case CLOUD_BLACK_SMOKE:
            case CLOUD_GREY_SMOKE:
            case CLOUD_BLUE_SMOKE:
            case CLOUD_PURPLE_SMOKE:
            case CLOUD_FOREST_FIRE:
            case CLOUD_PETRIFY:
            case CLOUD_RAIN:
            case CLOUD_DUST_TRAIL:
            case CLOUD_STORM:
                targets.push_back(*ri);
                break;
            default:
                break;
            }
        }
    }

    if (targets.empty())
    {
        mpr("You can't see any clouds you can empower.");
        return;
    }

    shuffle_array(targets);
    const int count = max(1, min((int)targets.size(),
                                 random2avg(you.skill(SK_INVOCATIONS), 2)));
    mgen_data mg;
    mg.summon_type = MON_SUMM_AID;
    mg.abjuration_duration = 1;
    mg.flags |= MG_FORCE_PLACE | MG_AUTOFOE;
    int placed = 0;
    for (unsigned int i = 0; placed < count && i < targets.size(); i++)
    {
        coord_def pos = targets[i];
        const unsigned short cloud = env.cgrid(pos);
        ASSERT(cloud != EMPTY_CLOUD);
        cloud_struct &cl = env.cloud[cloud];
        actor *agent = actor_by_mid(cl.source);
        mg.behaviour = !agent             ? BEH_NEUTRAL :
                       agent->is_player() ? BEH_FRIENDLY
                                          : SAME_ATTITUDE(agent->as_monster());
        mg.pos       = pos;
        switch (cl.type)
        {
        case CLOUD_FIRE:
        case CLOUD_FOREST_FIRE:
            mg.cls = MONS_FIRE_ELEMENTAL;
            break;
        case CLOUD_COLD:
        case CLOUD_RAIN:
            mg.cls = MONS_WATER_ELEMENTAL; // maybe ice beasts for cold?
            break;
        case CLOUD_PETRIFY:
        case CLOUD_DUST_TRAIL:
            mg.cls = MONS_EARTH_ELEMENTAL;
            break;
        case CLOUD_BLACK_SMOKE:
        case CLOUD_GREY_SMOKE:
        case CLOUD_BLUE_SMOKE:
        case CLOUD_PURPLE_SMOKE:
        case CLOUD_STORM:
            mg.cls = MONS_AIR_ELEMENTAL; // maybe sky beasts for storm?
            break;
        default:
            continue;
        }
        if (!create_monster(mg))
            continue;
        delete_cloud(cloud);
        placed++;
    }

    if (placed)
        mprf(MSGCH_GOD, "Clouds arounds you coalesce and take form!");
    else
        canned_msg(MSG_NOTHING_HAPPENS); // can this ever happen?
}

bool qazlal_disaster_area()
{
    bool friendlies = false;
    vector<coord_def> targets;
    vector<int> weights;
    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_ROUND, LOS_NO_TRANS, true);
         ri; ++ri)
    {
        if (!in_bounds(*ri) || cell_is_solid(*ri))
            continue;

        const monster_info* m = env.map_knowledge(*ri).monsterinfo();
        if (m && mons_att_wont_attack(m->attitude)
            && !mons_is_projectile(m->type))
        {
            friendlies = true;
        }
        const int dist = distance2(you.pos(), *ri);
        if (dist <= 8)
            continue;

        targets.push_back(*ri);
        weights.push_back(LOS_RADIUS_SQ - dist);
    }

    if (targets.empty())
    {
        mpr("There isn't enough space here!");
        return false;
    }

    if (friendlies
        && !yesno("There are friendlies around; are you sure you want to hurt "
                  "them?", true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    mprf(MSGCH_GOD, "Nature churns violently around you!");

    int count = max(1, min((int)targets.size(),
                            max(you.skill_rdiv(SK_INVOCATIONS, 1, 2),
                                random2avg(you.skill(SK_INVOCATIONS, 2), 2))));
    vector<coord_def> victims;
    for (int i = 0; i < count; i++)
    {
        if (targets.size() == 0)
            break;
        int which = choose_random_weighted(weights.begin(), weights.end());
        unsigned int j = 0;
        for (; j < victims.size(); j++)
            if (adjacent(targets[which], victims[j]))
                break;
        if (j == victims.size())
            qazlal_upheaval(targets[which], true);
        targets.erase(targets.begin() + which);
        weights.erase(weights.begin() + which);
    }
    scaled_delay(100);

    return true;
}

static map<ability_type, const sacrifice_def *> sacrifice_data_map;

void init_sac_index()
{
    for (unsigned int i = ABIL_FIRST_SACRIFICE; i <= ABIL_FINAL_SACRIFICE; ++i)
    {
        unsigned int sac_index = i - ABIL_FIRST_SACRIFICE;
        sacrifice_data_map[static_cast<ability_type>(i)] = &sac_data[sac_index];
    }
}

static const sacrifice_def &_get_sacrifice_def(ability_type sac)
{
    ASSERT_RANGE(sac, ABIL_FIRST_SACRIFICE, ABIL_FINAL_SACRIFICE+1);
    return *sacrifice_data_map[sac];
}

static vector<ability_type> _get_possible_sacrifices()
{
    vector<ability_type> possible_sacrifices;

    if (!player_mutation_level(MUT_NO_READ))
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_WORDS);
    if (!player_mutation_level(MUT_NO_DRINK) && you.species != SP_MUMMY)
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_DRINK);
    if (!player_mutation_level(MUT_NO_STEALTH))
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_STEALTH);
    if (!player_mutation_level(MUT_NO_ARTIFICE))
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_ARTIFICE);
    if (!player_mutation_level(MUT_NO_LOVE))
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_LOVE);
    if (!player_mutation_level(MUT_COWARDICE))
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_COURAGE);
    if (!player_mutation_level(MUT_NO_DODGING))
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_NIMBLENESS);
    if (!player_mutation_level(MUT_NO_ARMOUR) && you_can_wear(EQ_BODY_ARMOUR))
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_DURABILITY);
    if (!player_mutation_level(MUT_MISSING_HAND))
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_HAND);

    // Determine how many health mutations are left and which one to use
    ASSERT(you.props.exists("current_health_sacrifice"));
    CrawlVector &current_health_sacrifice
        = you.props["current_health_sacrifice"].get_vector();

    vector<mutation_type> possible_health_mutations;
    if (player_mutation_level(MUT_FRAIL) <= 2
        && you.innate_mutation[MUT_ROBUST] == 0) // block conflicts w/ DS
        possible_health_mutations.push_back(MUT_FRAIL);
    if (player_mutation_level(MUT_PHYSICAL_VULNERABILITY) <= 2)
        possible_health_mutations.push_back(MUT_PHYSICAL_VULNERABILITY);
    if (player_mutation_level(MUT_SLOW_REFLEXES) <= 2)
        possible_health_mutations.push_back(MUT_SLOW_REFLEXES);
    int num_health_mutations = possible_health_mutations.size();

    if (num_health_mutations > 0)
    {
        current_health_sacrifice.push_back(
        static_cast<int>(possible_health_mutations[
            random2(num_health_mutations)]));

        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_HEALTH);
    }

    // Determine how many essence mutations are left and which one to use
    ASSERT(you.props.exists("current_essence_sacrifice"));
    CrawlVector &current_essence_sacrifice
        = you.props["current_essence_sacrifice"].get_vector();

    vector<mutation_type> possible_essence_mutations;
    if (player_mutation_level(MUT_ANTI_WIZARDRY) <= 2)
        possible_essence_mutations.push_back(MUT_ANTI_WIZARDRY);
    if (player_mutation_level(MUT_MAGICAL_VULNERABILITY) <= 2)
        possible_essence_mutations.push_back(MUT_MAGICAL_VULNERABILITY);
    if (player_mutation_level(MUT_LOW_MAGIC) <= 2)
        possible_essence_mutations.push_back(MUT_LOW_MAGIC);
    int num_essence_mutations = possible_essence_mutations.size();

    if (num_essence_mutations > 0)
    {
        current_essence_sacrifice.push_back(
            static_cast<int>(possible_essence_mutations[
                random2(num_essence_mutations)]));
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_ESSENCE);
    }

    // Determine how many purity mutations are left and which one to use
    ASSERT(you.props.exists("current_purity_sacrifice"));
    CrawlVector &current_purity_sacrifice
        = you.props["current_purity_sacrifice"].get_vector();

    vector<mutation_type> possible_purity_mutations;
    if (player_mutation_level(MUT_SCREAM) <= 2)
        possible_purity_mutations.push_back(MUT_SCREAM);
    if (player_mutation_level(MUT_SLOW_HEALING) <= 2
        && !player_mutation_level(MUT_NO_DEVICE_HEAL)
        && you.innate_mutation[MUT_REGENERATION] == 0) //block conflicts
    {
        possible_purity_mutations.push_back(MUT_SLOW_HEALING);
    }
    if (player_mutation_level(MUT_NO_DEVICE_HEAL) <= 2
        && !player_mutation_level(MUT_SLOW_HEALING))
    {
        possible_purity_mutations.push_back(MUT_NO_DEVICE_HEAL);
    }
    possible_purity_mutations.push_back(MUT_DOPEY);
    possible_purity_mutations.push_back(MUT_CLUMSY);
    possible_purity_mutations.push_back(MUT_WEAK);

    int num_purity_mutations = possible_purity_mutations.size();
    if (num_purity_mutations > 0)
    {
        current_purity_sacrifice.push_back(
            static_cast<int>(possible_purity_mutations[
                random2(num_purity_mutations)]));
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_PURITY);
    }

    ASSERT(you.props.exists("current_arcane_sacrifices"));
    CrawlVector &current_arcane_sacrifices
        = you.props["current_arcane_sacrifices"].get_vector();

    vector<mutation_type> possible_minor_mutations;
    vector<mutation_type> possible_medium_mutations;
    vector<mutation_type> possible_major_mutations;
    int num_major_mutations;
    int num_medium_mutations;
    int num_minor_mutations;

    if (!player_mutation_level(MUT_NO_CHARM_MAGIC))
        possible_major_mutations.push_back(MUT_NO_CHARM_MAGIC);
    if (!player_mutation_level(MUT_NO_CONJURATION_MAGIC))
        possible_major_mutations.push_back(MUT_NO_CONJURATION_MAGIC);
    if (!player_mutation_level(MUT_NO_SUMMONING_MAGIC)
        && !player_mutation_level(MUT_NO_LOVE))
        {
            possible_major_mutations.push_back(MUT_NO_SUMMONING_MAGIC);
        }
    if (!player_mutation_level(MUT_NO_TRANSLOCATION_MAGIC))
        possible_major_mutations.push_back(MUT_NO_TRANSLOCATION_MAGIC);
    num_major_mutations = possible_major_mutations.size();
    current_arcane_sacrifices.push_back(
        static_cast<int>(possible_major_mutations[
            random2(num_major_mutations)]));

    if (!player_mutation_level(MUT_NO_TRANSMUTATION_MAGIC))
        possible_medium_mutations.push_back(MUT_NO_TRANSMUTATION_MAGIC);
    if (!player_mutation_level(MUT_NO_NECROMANCY_MAGIC))
        possible_medium_mutations.push_back(MUT_NO_NECROMANCY_MAGIC);
    if (!player_mutation_level(MUT_NO_HEXES_MAGIC))
        possible_medium_mutations.push_back(MUT_NO_HEXES_MAGIC);
    num_medium_mutations = possible_medium_mutations.size();
    current_arcane_sacrifices.push_back(
        static_cast<int>(possible_medium_mutations[
            random2(num_medium_mutations)]));

    if (!player_mutation_level(MUT_NO_AIR_MAGIC))
        possible_minor_mutations.push_back(MUT_NO_AIR_MAGIC);
    if (!player_mutation_level(MUT_NO_EARTH_MAGIC))
        possible_minor_mutations.push_back(MUT_NO_EARTH_MAGIC);
    if (!player_mutation_level(MUT_NO_FIRE_MAGIC))
        possible_minor_mutations.push_back(MUT_NO_FIRE_MAGIC);
    if (!player_mutation_level(MUT_NO_ICE_MAGIC))
        possible_minor_mutations.push_back(MUT_NO_ICE_MAGIC);
    if (!player_mutation_level(MUT_NO_POISON_MAGIC))
        possible_minor_mutations.push_back(MUT_NO_POISON_MAGIC);
    num_minor_mutations = possible_minor_mutations.size();
    current_arcane_sacrifices.push_back(
        static_cast<int>(possible_minor_mutations[
            random2(num_minor_mutations)]));

    if (player_mutation_level(MUT_NO_AIR_MAGIC)
        + player_mutation_level(MUT_NO_CHARM_MAGIC)
        + player_mutation_level(MUT_NO_CONJURATION_MAGIC)
        + player_mutation_level(MUT_NO_EARTH_MAGIC)
        + player_mutation_level(MUT_NO_FIRE_MAGIC)
        + player_mutation_level(MUT_NO_HEXES_MAGIC)
        + player_mutation_level(MUT_NO_ICE_MAGIC)
        + player_mutation_level(MUT_NO_NECROMANCY_MAGIC)
        + player_mutation_level(MUT_NO_POISON_MAGIC)
        + player_mutation_level(MUT_NO_SUMMONING_MAGIC)
        + player_mutation_level(MUT_NO_TRANSLOCATION_MAGIC)
        + player_mutation_level(MUT_NO_TRANSMUTATION_MAGIC)
        < 3)
    {
        possible_sacrifices.push_back(ABIL_RU_SACRIFICE_ARCANA);
    }

    return possible_sacrifices;
}

static const char* _arcane_mutation_to_school_name(mutation_type mutation)
{
    switch (mutation)
    {
        case MUT_NO_AIR_MAGIC:
            return "Air Magic";
        case MUT_NO_CHARM_MAGIC:
            return "Charms";
        case MUT_NO_CONJURATION_MAGIC:
            return "Conjurations";
        case MUT_NO_EARTH_MAGIC:
            return "Earth Magic";
        case MUT_NO_FIRE_MAGIC:
            return "Fire Magic";
        case MUT_NO_HEXES_MAGIC:
            return "Hexes";
        case MUT_NO_ICE_MAGIC:
            return "Ice Magic";
        case MUT_NO_NECROMANCY_MAGIC:
            return "Necromancy";
        case MUT_NO_POISON_MAGIC:
            return "Poison Magic";
        case MUT_NO_SUMMONING_MAGIC:
            return "Summoning";
        case MUT_NO_TRANSLOCATION_MAGIC:
            return "Translocations";
        case MUT_NO_TRANSMUTATION_MAGIC:
            return "Transmutations";
        default:
            return "N/A";
    }
}

static const skill_type _arcane_mutation_to_skill(mutation_type mutation)
{
    switch (mutation)
    {
        case MUT_NO_AIR_MAGIC:
            return SK_AIR_MAGIC;
        case MUT_NO_CHARM_MAGIC:
            return SK_CHARMS;
        case MUT_NO_CONJURATION_MAGIC:
            return SK_CONJURATIONS;
        case MUT_NO_EARTH_MAGIC:
            return SK_EARTH_MAGIC;
        case MUT_NO_FIRE_MAGIC:
            return SK_FIRE_MAGIC;
        case MUT_NO_HEXES_MAGIC:
            return SK_HEXES;
        case MUT_NO_ICE_MAGIC:
            return SK_ICE_MAGIC;
        case MUT_NO_NECROMANCY_MAGIC:
            return SK_NECROMANCY;
        case MUT_NO_POISON_MAGIC:
            return SK_POISON_MAGIC;
        case MUT_NO_SUMMONING_MAGIC:
            return SK_SUMMONINGS;
        case MUT_NO_TRANSLOCATION_MAGIC:
            return SK_TRANSLOCATIONS;
        case MUT_NO_TRANSMUTATION_MAGIC:
            return SK_TRANSMUTATIONS;
        default:
            return SK_NONE;
    }
}

static int _piety_for_skill(skill_type skill)
{
    return skill_exp_needed(you.skills[skill], skill, you.species) / 500;
}

#define AS_MUT(csv) (static_cast<mutation_type>((csv).get_int()))

static int _get_sacrifice_piety(ability_type sac)
{
    const sacrifice_def &sac_def = _get_sacrifice_def(sac);
    int piety_gain = sac_def.base_piety;
    ability_type sacrifice = sac_def.sacrifice;
    mutation_type mut = MUT_NON_MUTATION;
    int num_sacrifices = 0;
    const int piety_for_lost_muts = 15;
    const int piety_for_lost_stat_muts = 6;


    // Initialize data

    if (sac_def.sacrifice_vector)
    {
        ASSERT(you.props.exists(sac_def.sacrifice_vector));
        CrawlVector &sacrifice_muts =
            you.props[sac_def.sacrifice_vector].get_vector();
        num_sacrifices = sacrifice_muts.size();
        // mut can only meaningfully be set here if we have exactly one.
        if (num_sacrifices == 1)
            mut = AS_MUT(sacrifice_muts[0]);
    }
    else
        mut = sac_def.mutation;

    // Increase piety each skill point removed.
    if (sacrifice == ABIL_RU_SACRIFICE_ARCANA)
    {
        skill_type arcane_skill;
        mutation_type arcane_mut;
        CrawlVector &sacrifice_muts =
            you.props[sac_def.sacrifice_vector].get_vector();
        for (int i = 0; i < num_sacrifices; i++)
        {
            arcane_mut = AS_MUT(sacrifice_muts[i]);
            arcane_skill = _arcane_mutation_to_skill(arcane_mut);
            piety_gain += _piety_for_skill(arcane_skill);
        }
    }
    else if (sac_def.sacrifice_skill != SK_NONE)
        piety_gain += _piety_for_skill(sac_def.sacrifice_skill);

    switch (sacrifice)
    {
        case ABIL_RU_SACRIFICE_ESSENCE:
            if (mut == MUT_LOW_MAGIC)
                piety_gain += 12;
            else if (mut == MUT_MAGICAL_VULNERABILITY)
                piety_gain += 28;
            else
                piety_gain += 16;
            break;
        case ABIL_RU_SACRIFICE_PURITY:
            if (mut == MUT_WEAK
                || mut == MUT_CLUMSY
                || mut == MUT_DOPEY)
            {
                if (player_mutation_level(MUT_STRONG) && mut == MUT_WEAK)
                piety_gain += 8;
            }
            // the other sacrifices get sharply worse if you already
            // have levels of them.
            else if (player_mutation_level(mut) == 2)
                piety_gain += 28;
            else if (player_mutation_level(mut) == 1)
                piety_gain += 20;
            else
                piety_gain += 12;

            break;
        case ABIL_RU_SACRIFICE_ARTIFICE:
            if (player_mutation_level(MUT_NO_LOVE))
                piety_gain -= 10; // You've already lost some value here
            break;
        case ABIL_RU_SACRIFICE_NIMBLENESS:
            if (player_mutation_level(MUT_NO_ARMOUR))
                piety_gain += 20;
            else if (you.species == SP_OCTOPODE
                    || you.species == SP_FELID
                    || player_genus(GENPC_DRACONIAN))
            {
                piety_gain += 28; // this sacrifice is worse for these races
            }
            break;
        case ABIL_RU_SACRIFICE_DURABILITY:
            if (player_mutation_level(MUT_NO_DODGING))
                piety_gain += 20;
            break;
        case ABIL_RU_SACRIFICE_LOVE:
            if (player_mutation_level(MUT_NO_SUMMONING_MAGIC)
                && player_mutation_level(MUT_NO_ARTIFICE))
            {
                // this is virtually useless, aside from zot_tub
                piety_gain -= 19;
            }
            else if (player_mutation_level(MUT_NO_SUMMONING_MAGIC)
                || player_mutation_level(MUT_NO_ARTIFICE))
            {
                piety_gain -= 10;
            }
            break;
        default:
            break;
    }

    // Award piety for any mutations removed by adding new innate muts
    // These can only be removed positive mutations, so we'll always give piety.
    if (sacrifice == ABIL_RU_SACRIFICE_PURITY
        || sacrifice == ABIL_RU_SACRIFICE_HEALTH
        || sacrifice == ABIL_RU_SACRIFICE_ESSENCE)
    {
        int existing_mut_val = 0;
        if (mut == MUT_STRONG || mut == MUT_CLEVER || mut == MUT_AGILE)
            existing_mut_val = piety_for_lost_stat_muts;
        else
            existing_mut_val = piety_for_lost_muts;
        piety_gain += existing_mut_val * mut_check_conflict(mut);
    }

    return piety_gain;
}

// Remove the offer of sacrifices after they've been offered for sufficient
// time or it's time to offer something new.
static void _ru_expire_sacrifices()
{
    ASSERT(you.props.exists("available_sacrifices"));
    ASSERT(you.props.exists("current_health_sacrifice"));
    ASSERT(you.props.exists("current_essence_sacrifice"));
    ASSERT(you.props.exists("current_purity_sacrifice"));
    ASSERT(you.props.exists("current_arcane_sacrifices"));

    CrawlVector &available_sacrifices
        = you.props["available_sacrifices"].get_vector();
    CrawlVector &current_health_sacrifice
        = you.props["current_health_sacrifice"].get_vector();
    CrawlVector &current_essence_sacrifice
        = you.props["current_essence_sacrifice"].get_vector();
    CrawlVector &current_purity_sacrifice
        = you.props["current_purity_sacrifice"].get_vector();
    CrawlVector &current_arcane_sacrifices
        = you.props["current_arcane_sacrifices"].get_vector();

    available_sacrifices.clear();
    current_health_sacrifice.clear();
    current_essence_sacrifice.clear();
    current_purity_sacrifice.clear();
    current_arcane_sacrifices.clear();
}

// Pick three new sacrifices to offer to the player. They should be distinct
// from one another and not offer duplicates of some options.
void ru_offer_new_sacrifices()
{
    _ru_expire_sacrifices();

    vector<ability_type> possible_sacrifices = _get_possible_sacrifices();

    // for now we'll just pick three at random
    int num_sacrifices = possible_sacrifices.size();

    // This can't happen outside wizmode, but may as well handle gracefully
    if (num_sacrifices < 3)
        return;

    simple_god_message(" believes you are ready to make a new sacrifice.");
                    more();

    // try to get three distinct sacrifices
    int lesser_sacrifice;
    int sacrifice = -1;
    int greater_sacrifice = -1;
    int number_of_tries = 0;
    int max_tries = 20;
    int max_overpiety = 170;

    do
    {
        lesser_sacrifice = random2(num_sacrifices);
        number_of_tries += 1;
        if (number_of_tries == max_tries)
        {
            number_of_tries = 0;
            max_overpiety += 3;
        }
    }
    while (lesser_sacrifice == -1
        || you.piety + _get_sacrifice_piety(possible_sacrifices[lesser_sacrifice]) > max_overpiety);

    number_of_tries = 0;
    do
    {
        sacrifice = random2(num_sacrifices);
        number_of_tries += 1;
        if (number_of_tries == max_tries)
        {
            number_of_tries = 0;
            max_overpiety += 3;
        }
    }
    while (sacrifice == -1
            || sacrifice == lesser_sacrifice
            || you.piety + _get_sacrifice_piety(possible_sacrifices[sacrifice]) > max_overpiety);

    number_of_tries = 0;
    do
    {
        greater_sacrifice = random2(num_sacrifices);
        number_of_tries += 1;
        if (number_of_tries == max_tries)
        {
            number_of_tries = 0;
            max_overpiety += 3;
        }
    }
    while (greater_sacrifice == -1
            || greater_sacrifice == lesser_sacrifice
            || greater_sacrifice == sacrifice
            || you.piety + _get_sacrifice_piety(possible_sacrifices[greater_sacrifice]) > max_overpiety);

    ASSERT(you.props.exists("available_sacrifices"));
    CrawlVector &available_sacrifices
        = you.props["available_sacrifices"].get_vector();

    // set the new abilities
    available_sacrifices.push_back(
            static_cast<int>(possible_sacrifices[lesser_sacrifice]));
    available_sacrifices.push_back(
            static_cast<int>(possible_sacrifices[sacrifice]));
    available_sacrifices.push_back(
            static_cast<int>(possible_sacrifices[greater_sacrifice]));
}

static const char* _describe_sacrifice_piety_gain(int piety_gain)
{
    if (piety_gain >= 40)
        return "an incredible";
    else if (piety_gain >= 29)
        return "a major";
    else if (piety_gain >= 21)
        return "a significant";
    else if (piety_gain >= 13)
        return "a modest";
    else
        return "a trivial";
}

static void _apply_ru_sacrifice(mutation_type sacrifice)
{
    perma_mutate(sacrifice, 1, "Ru sacrifice");
    you.sacrifices[sacrifice] += 1;
}

static bool _execute_sacrifice(int piety_gain, const char* message)
{
    mprf("Ru asks you to %s.", message);
    mprf("This is %s sacrifice.", _describe_sacrifice_piety_gain(piety_gain));
    if (!yesno("Do you really want to make this sacrifice?",
               false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }
    return true;
}

static void _ru_kill_skill(skill_type skill)
{
    change_skill_points(skill, -you.skill_points[skill], true);
    you.stop_train.insert(skill);
}

static void _extra_sacrifice_code(ability_type sac)
{
    const sacrifice_def &sac_def = _get_sacrifice_def(sac);
    if (sac_def.sacrifice == ABIL_RU_SACRIFICE_HAND)
    {
        equipment_type ring_slot;

        if (you.species == SP_OCTOPODE)
            ring_slot = EQ_RING_EIGHT;
        else
            ring_slot = EQ_LEFT_RING;

        item_def* const shield = you.slot_item(EQ_SHIELD, true);
        item_def* const weapon = you.slot_item(EQ_WEAPON, true);
        item_def* const ring = you.slot_item(ring_slot, true);
        int ring_inv_slot = you.equip[ring_slot];
        bool open_ring_slot = false;

        // Drop your shield if there is one
        if (shield != NULL)
        {
            mprf("You can no longer hold %s!",
                shield->name(DESC_YOUR).c_str());
            unequip_item(EQ_SHIELD);
        }

        // And your two-handed weapon
        if (weapon != NULL)
        {
            if (you.hands_reqd(*weapon) == HANDS_TWO)
            {
                mprf("You can no longer hold %s!",
                    weapon->name(DESC_YOUR).c_str());
                unequip_item(EQ_WEAPON);
            }
        }

        // And one ring
        if (ring != NULL)
        {
            if (you.species == SP_OCTOPODE)
            {
                for (int eq = EQ_RING_ONE; eq <= EQ_RING_SEVEN; eq++)
                {
                    if (!you.slot_item(static_cast<equipment_type>(eq), true))
                    {
                        open_ring_slot = true;
                        break;
                    }
                }
            }
            else
            {
                if (!you.slot_item(static_cast<equipment_type>(
                    EQ_RIGHT_RING), true))
                {
                    open_ring_slot = true;
                }
            }

            mprf("You can no longer wear %s!",
                ring->name(DESC_YOUR).c_str());
            unequip_item(ring_slot);
            if (open_ring_slot)
            {
                mprf("You put %s back on %s %s!",
                     ring->name(DESC_YOUR).c_str(),
                     (you.species == SP_OCTOPODE ? "another" : "your other"),
                     you.hand_name(true).c_str());
                puton_ring(ring_inv_slot, false);
            }
        }
    }
}

bool ru_do_sacrifice(ability_type sac)
{
    const sacrifice_def &sac_def = _get_sacrifice_def(sac);
    bool variable_sac;
    mutation_type mut = MUT_NON_MUTATION;
    int num_sacrifices;
    string offer_text;
    string mile_text;
    string sac_text;
    const bool is_sac_arcana = sac == ABIL_RU_SACRIFICE_ARCANA;
    int piety_gain = 0;

    // For variable sacrifices, we need to compose the text that will be
    // displayed at the time of sacrifice offer and as a milestone if the
    // sacrifice is accepted. We also need to figure out piety.
    if (sac_def.sacrifice_vector)
    {
        variable_sac = true;
        ASSERT(you.props.exists(sac_def.sacrifice_vector));
        CrawlVector &sacrifice_muts =
            you.props[sac_def.sacrifice_vector].get_vector();
        num_sacrifices = sacrifice_muts.size();

        for (int i = 0; i < num_sacrifices; i++)
        {
            mut = AS_MUT(sacrifice_muts[i]);

            // format the text that will be displayed
            if (is_sac_arcana)
            {
                if (i == num_sacrifices - 1)
                {
                    sac_text = make_stringf("%sand %s", sac_text.c_str(),
                        _arcane_mutation_to_school_name(mut));
                }
                else
                {
                    sac_text = make_stringf("%s%s, ", sac_text.c_str(),
                        _arcane_mutation_to_school_name(mut));
                }
            }
            else
                sac_text = static_cast<string>(mutation_desc_for_text(mut));
        }
        offer_text = make_stringf("%s: %s", sac_def.sacrifice_text,
            sac_text.c_str());
        mile_text = make_stringf("%s: %s.", sac_def.milestone_text,
            sac_text.c_str());
    }
    else
    {
        variable_sac = false;
        mut = sac_def.mutation;
        num_sacrifices = 1;
        const char* handtxt = "";
        if (sac == ABIL_RU_SACRIFICE_HAND)
            handtxt = you.hand_name(true).c_str();

        offer_text = make_stringf("%s%s", sac_def.sacrifice_text, handtxt);
        mile_text = make_stringf("%s.", sac_def.milestone_text);
    }

    piety_gain = _get_sacrifice_piety(sac);

    // get confirmation that the sacrifice is desired.
    if (!_execute_sacrifice(piety_gain, offer_text.c_str()))
        return false;

    // Apply the sacrifice, starting by mutating the player.
    if (variable_sac)
    {
        CrawlVector &sacrifice_muts =
            you.props[sac_def.sacrifice_vector].get_vector();
        for (int i = 0; i < num_sacrifices; i++)
        {
            mut = AS_MUT(sacrifice_muts[i]);
            _apply_ru_sacrifice(mut);
        }
    }
    else
        _apply_ru_sacrifice(mut);

    // Remove any no-longer-usable skills.
    if (is_sac_arcana)
    {
        skill_type arcane_skill;
        mutation_type arcane_mut;
        CrawlVector &sacrifice_muts =
            you.props[sac_def.sacrifice_vector].get_vector();
        for (int i = 0; i < num_sacrifices; i++)
        {
            arcane_mut = AS_MUT(sacrifice_muts[i]);
            arcane_skill = _arcane_mutation_to_skill(arcane_mut);
            _ru_kill_skill(arcane_skill);
        }
    }
    else if (sac_def.sacrifice_skill != SK_NONE)
        _ru_kill_skill(sac_def.sacrifice_skill);

    mark_milestone("sacrifice", mile_text.c_str());

    // Any special handling that's needed.
    _extra_sacrifice_code(sac);

    // Update how many Ru sacrifices you have. This is used to avoid giving the
    // player extra silver damage.
    if (you.props.exists("num_sacrifice_muts"))
    {
        you.props["num_sacrifice_muts"] = num_sacrifices +
            you.props["num_sacrifice_muts"].get_int();
    }
    else
        you.props["num_sacrifice_muts"] = num_sacrifices;

    // Randomize piety gain very slightly to prevent counting.
    int new_piety = you.piety + piety_gain + random2(3);
    if (new_piety > piety_breakpoint(5))
        new_piety = piety_breakpoint(5);
    set_piety(new_piety);

    if (you.piety == piety_breakpoint(5))
        simple_god_message(" indicates that your awakening is complete.");

    // Clean up.
    _ru_expire_sacrifices();
    ru_reset_sacrifice_timer(true);
    redraw_screen(); // pretty much everything could have changed
    return true;
}

bool ru_reject_sacrifices()
{
    if (!yesno("Do you really want to reject the sacrifices Ru is offering?",
               false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    _ru_expire_sacrifices();
    ru_reset_sacrifice_timer(false);
    simple_god_message(" will take longer to evaluate your readiness.");
    return true;
}

void ru_reset_sacrifice_timer(bool clear_timer)
{
    ASSERT(you.props.exists("ru_progress_to_next_sacrifice"));
    ASSERT(you.props.exists("ru_sacrifice_delay"));

    // raise the delay if there's an active sacrifice, and more so the more
    // often you pass on a sacrifice and the more piety you have.
    int base_delay = 80;
    int delay = you.props["ru_sacrifice_delay"].get_int();
    int added_delay;
    if (clear_timer)
    {
        added_delay = 0;
        delay = base_delay;
    }
    else
        added_delay = (90 + max(100, static_cast<int>(you.piety)) - 100) / 3;

    delay = div_rand_round((delay + added_delay) * (3 + you.faith()), 3);
    if (crawl_state.game_is_sprint())
        delay /= SPRINT_MULTIPLIER;

    you.props["ru_sacrifice_delay"] = delay;
}

// Check to see if you're eligible to retaliate.
//Your chance of eligiblity scales with piety.
bool will_ru_retaliate()
{
    // Scales up to a 33% chance of retribution
    return you_worship(GOD_RU)
           && you.piety >= piety_breakpoint(1)
           && crawl_state.which_god_acting() != GOD_RU
           && one_chance_in(div_rand_round(480, you.piety));
}

// Power of retribution increases with damage, decreases with monster HD.
void ru_do_retribution(monster* mons, int damage)
{
    int power = max(0, random2(div_rand_round(you.piety*10, 32))
        + damage - (2 * mons->get_hit_dice()));
    const actor* act = &you;

    if (power > 50 && (mons->has_spells() || mons->is_actual_spellcaster()))
    {
        simple_monster_message(mons, " is muted in retribution by your will!",
            MSGCH_GOD);
        mons->add_ench(mon_enchant(ENCH_MUTE, 1, act, power+random2(120)));
    }
    else if (power > 35)
    {
        simple_monster_message(mons, " is paralysed in retribution by your will!",
            MSGCH_GOD);
        mons->add_ench(mon_enchant(ENCH_PARALYSIS, 1, act, power+random2(60)));
    }
    else if (power > 25)
    {
        simple_monster_message(mons, " is slowed in retribution by your will!",
            MSGCH_GOD);
        mons->add_ench(mon_enchant(ENCH_SLOW, 1, act, power+random2(100)));
    }
    else if (power > 10 && mons_can_be_blinded(mons->type))
    {
        simple_monster_message(mons, " is blinded in retribution by your will!",
            MSGCH_GOD);
        mons->add_ench(mon_enchant(ENCH_BLIND, 1, act, power+random2(100)));
    }
    else if (power > 0)
    {
        simple_monster_message(mons, " is illuminated in retribution by your will!",
            MSGCH_GOD);
        mons->add_ench(mon_enchant(ENCH_CORONA, 1, act, power+random2(150)));
    }
}

void ru_draw_out_power()
{
    mpr("You are restored by drawing out deep reserves of power within.");

    //Escape nets and webs
    int net = get_trapping_net(you.pos());
    if (net == NON_ITEM)
    {
        trap_def *trap = find_trap(you.pos());
        if (trap && trap->type == TRAP_WEB)
        {
            destroy_trap(you.pos());
            mpr("You burst free from the webs!");
        }
    }
    else
    {
        destroy_item(net);
        mpr("You burst free from the net!");
    }

    // Escape constriction
    you.stop_being_constricted(false);

    // cancel petrification/confusion/slow
    you.duration[DUR_CONF] = 0;
    you.duration[DUR_SLOW] = 0;
    you.duration[DUR_PETRIFYING] = 0;

    you.attribute[ATTR_HELD] = 0;
    you.redraw_quiver = true;
    you.redraw_evasion = true;

    inc_hp(div_rand_round(you.piety, 16)
           + roll_dice(div_rand_round(you.piety, 20), 6));
    inc_mp(div_rand_round(you.piety, 48)
           + roll_dice(div_rand_round(you.piety, 40), 4));
    drain_player(25, false, true);
}

bool ru_power_leap()
{
    ASSERT(!crawl_state.game_is_arena());

    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("You can't repeat power leap.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return false;
    }

    // query for location:
    int range = 3;
    int explosion_size = 1;
    dist beam;
    bolt fake_beam;

    while (1)
    {
        targetter_smite tgt(&you, range, explosion_size, explosion_size);
        if (!spell_direction(beam, fake_beam, DIR_LEAP, TARG_ANY,
                             range, false, false, false, NULL,
                             "Aiming: <white>Power Leap</white>", true,
                             &tgt)
            && crawl_state.seen_hups)
        {
            clear_messages();
            mpr("Cancelling leap due to HUP.");
            return false;
        }

        if (!beam.isValid || beam.target == you.pos())
            return false;         // early return

        monster* beholder = you.get_beholder(beam.target);
        if (beholder)
        {
            clear_messages();
            mprf("You cannot leap away from %s!",
                 beholder->name(DESC_THE, true).c_str());
            continue;
        }

        monster* fearmonger = you.get_fearmonger(beam.target);
        if (fearmonger)
        {
            clear_messages();
            mprf("You cannot leap closer to %s!",
                 fearmonger->name(DESC_THE, true).c_str());
            continue;
        }

        monster* mons = monster_at(beam.target);
        if (mons && you.can_see(mons))
        {
            clear_messages();
            mpr("You can't leap on top of the monster!");
            continue;
        }

        if (grd(beam.target) == DNGN_OPEN_SEA)
        {
            clear_messages();
            mpr("You can't leap into the sea!");
            continue;
        }
        else if (grd(beam.target) == DNGN_LAVA_SEA)
        {
            clear_messages();
            mpr("You can't leap into the sea of lava!");
            continue;
        }
        else if (!check_moveto(beam.target, "leap"))
        {
            // try again (messages handled by check_moveto)
        }
        else if (you.see_cell_no_trans(beam.target))
        {
            // Grid in los, no problem.
            break;
        }
        else if (you.trans_wall_blocking(beam.target))
        {
            clear_messages();
            mpr("There's something in the way!");
        }
        else
        {
            clear_messages();
            mpr("You can only leap to visible locations.");
        }
    }

    bool return_val = false;

    if (you.attempt_escape(2)) // I'm hoping this returns true if not constrict
    {
        if (cell_is_solid(beam.target) || monster_at(beam.target))
            mpr("Something unexpectedly blocked you, preventing you from leaping!");
        else
            move_player_to_grid(beam.target, false);
    }

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();
    return_val = true;

    bolt wave;
    wave.thrower = KILL_YOU;
    wave.name = "power leap";
    wave.source_name = "you";
    wave.source_id = MID_PLAYER;
    wave.flavour = BEAM_VISUAL;
    wave.colour = BROWN;
    wave.glyph = dchar_glyph(DCHAR_EXPLOSION);
    wave.range = 1;
    wave.ex_size = 1;
    wave.is_explosion = true;
    wave.source = you.pos();
    wave.target = you.pos();
    wave.hit = AUTOMATIC_HIT;
    wave.loudness = 2;
    wave.explode();

    // we need to exempt the player from damage.
    for (adjacent_iterator ai(you.pos(), false); ai; ++ai)
    {
        monster* mon = monster_at(*ai);
        if (mon == NULL || mons_is_projectile(mon->type) || mon->friendly())
            continue;
        ASSERT(mon);

        //damage scales with XL amd piety
        mon->hurt((actor*)&you, roll_dice(1 + div_rand_round(you.piety *
            (54 + you.experience_level), 777), 3),
            BEAM_ENERGY, true);
    }

    return return_val;
}

static int _apocalypseable(coord_def where, int pow, int, actor* agent)
{
    monster* mon = monster_at(where);
    if (mon == NULL || mons_is_projectile(mon->type) || mon->friendly())
        return 0;
    return 1;
}

static int _apply_apocalypse(coord_def where, int pow, int dummy, actor* agent)
{
    if (!_apocalypseable(where, pow, dummy, agent))
        return 0;
    monster* mons = monster_at(where);
    ASSERT(mons);

    int dmg = 10;
    //damage scales with XL amd piety
    int die_size = 1 + div_rand_round(pow * (54 + you.experience_level), 648);
    int effect = random2(5);

    if (mons_is_firewood(mons))
        effect = 99; // > 2 is just damage -- no slowed toadstools

    switch (effect)
    {
        case 0:
            if (mons->has_spells() || mons->is_actual_spellcaster())
            {
                simple_monster_message(mons, " is rendered silent by the truth!");
                mons->add_ench(mon_enchant(ENCH_MUTE, 1, agent, 120 + random2(160)));
                dmg += roll_dice(die_size, 4);
                break;
            } // if not a spellcaster, fall through to paralysis.

        case 1:
            simple_monster_message(mons, " is paralysed by terrible understanding!");
            mons->add_ench(mon_enchant(ENCH_PARALYSIS, 1, agent, 80 + random2(60)));
            dmg += roll_dice(die_size, 4);
            break;

        case 2:
            simple_monster_message(mons, " slows down under the weight of truth!");
            mons->add_ench(mon_enchant(ENCH_SLOW, 1, agent, 100 + random2(100)));
            dmg += roll_dice(die_size, 6);
            break;

        default:
            dmg += roll_dice(die_size, 8);
            break;
    }
    mons->hurt(agent, dmg, BEAM_ENERGY, true);

    return 1;
}

bool ru_apocalypse()
{
    int count = apply_area_visible(_apocalypseable, you.piety, &you);
    if (!count)
    {
        if (!yesno("There are no visible enemies. Unleash your apocalypse anyway?",
            true, 'n'))
        {
            return false;
        }
    }
    mpr("You reveal the great annihilating truth to your foes!");
    noisy(30, you.pos());
    apply_area_visible(_apply_apocalypse, you.piety, &you);
    drain_player(100,false, true);
    return true;
}

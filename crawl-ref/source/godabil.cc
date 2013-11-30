/**
 * @file
 * @brief God-granted abilities.
**/

#include "AppHdr.h"

#include <queue>
#include <sstream>

#include "areas.h"
#include "artefact.h"
#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dactions.h"
#include "effects.h"
#include "env.h"
#include "files.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "godcompanions.h"
#include "goditem.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "losglobal.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "mon-stuff.h"
#include "mutation.h"
#include "notes.h"
#include "ouch.h"
#include "place.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "skill_menu.h"
#include "shopping.h"
#include "shout.h"
#include "spl-book.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "target.h"
#include "terrain.h"
#include "unwind.h"
#include "view.h"
#include "viewgeom.h"

#ifdef USE_TILE
#include "tiledef-main.h"
#endif

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

typedef FixedVector<int, NUM_RECITE_TYPES> recite_counts;
// Check whether this monster might be influenced by Recite.
// Returns 0, if no monster found.
// Returns 1, if eligible monster found.
// Returns -1, if monster already affected or too dumb to understand.
static int _zin_check_recite_to_single_monster(const monster *mon,
                                               recite_counts &eligibility)
{
    ASSERT(mon);

    // Can't recite if they were recently recited to.
    if (mon->has_ench(ENCH_RECITE_TIMER))
        return -1;

    const mon_holy_type holiness = mon->holiness();

    // Can't recite at plants or golems.
    if (holiness == MH_PLANT || holiness == MH_NONLIVING)
        return -1;

    eligibility.init(0);

    // Recitations are based on monster::is_unclean, but are NOT identical to it,
    // because that lumps all forms of uncleanliness together. We want to specify.

    // Anti-chaos prayer:

    // Hits some specific insane or shapeshifted uniques.
    if (mon->type == MONS_CRAZY_YIUF
        || mon->type == MONS_PSYCHE
        || mon->type == MONS_GASTRONOK)
    {
        eligibility[RECITE_CHAOTIC]++;
    }

    // Hits monsters that have chaotic spells memorized.
    if (mon->has_chaotic_spell() && mon->is_actual_spellcaster())
        eligibility[RECITE_CHAOTIC]++;

    // Hits monsters with 'innate' chaos.
    if (mon->is_chaotic())
        eligibility[RECITE_CHAOTIC]++;

    // Hits monsters that are worshipers of a chaotic god.
    if (is_chaotic_god(mon->god))
        eligibility[RECITE_CHAOTIC]++;

    // Hits (again) monsters that are priests of a chaotic god.
    if (is_chaotic_god(mon->god) && mon->is_priest())
        eligibility[RECITE_CHAOTIC]++;

    // Anti-impure prayer:

    // Hits monsters that have unclean spells memorized.
    if (mon->has_unclean_spell())
        eligibility[RECITE_IMPURE]++;

    // Hits monsters that desecrate the dead.
    if (mons_eats_corpses(mon))
        eligibility[RECITE_IMPURE]++;

    // Hits corporeal undead, which are a perversion of natural form.
    if (holiness == MH_UNDEAD && !mon->is_insubstantial())
        eligibility[RECITE_IMPURE]++;

    // Hits monsters that have these brands.
    if (mon->has_attack_flavour(AF_VAMPIRIC))
        eligibility[RECITE_IMPURE]++;
    if (mon->has_attack_flavour(AF_DISEASE))
        eligibility[RECITE_IMPURE]++;
    if (mon->has_attack_flavour(AF_HUNGER))
        eligibility[RECITE_IMPURE]++;
    if (mon->has_attack_flavour(AF_ROT))
        eligibility[RECITE_IMPURE]++;
    if (mon->has_attack_flavour(AF_STEAL))
        eligibility[RECITE_IMPURE]++;
    if (mon->has_attack_flavour(AF_STEAL_FOOD))
        eligibility[RECITE_IMPURE]++;
    if (mon->has_attack_flavour(AF_PLAGUE))
        eligibility[RECITE_IMPURE]++;

    // Being naturally mutagenic isn't good either.
    corpse_effect_type ce = mons_corpse_effect(mon->type);
    if ((ce == CE_ROT || ce == CE_MUTAGEN) && !mon->is_chaotic())
        eligibility[RECITE_IMPURE]++;

    // Death drakes and rotting devils get a bump to uncleanliness.
    if (mon->type == MONS_ROTTING_DEVIL || mon->type == MONS_DEATH_DRAKE)
        eligibility[RECITE_IMPURE]++;

    // Sanity check: if a monster is 'really' natural, don't consider it impure.
    if (mons_intel(mon) < I_NORMAL
        && (holiness == MH_NATURAL || holiness == MH_PLANT)
        && mon->type != MONS_UGLY_THING
        && mon->type != MONS_VERY_UGLY_THING
        && mon->type != MONS_DEATH_DRAKE)
    {
        eligibility[RECITE_IMPURE] = 0;
    }

    // Don't allow against enemies that are already affected by an
    // earlier recite type.
    if (eligibility[RECITE_CHAOTIC] > 0)
        eligibility[RECITE_IMPURE] = 0;

    // Anti-unholy prayer

    // Hits demons and incorporeal undead.
    if (holiness == MH_UNDEAD && mon->is_insubstantial()
        || holiness == MH_DEMONIC)
    {
        eligibility[RECITE_UNHOLY]++;
    }

    // Don't allow against enemies that are already affected by an
    // earlier recite type.
    if (eligibility[RECITE_CHAOTIC] > 0
        || eligibility[RECITE_IMPURE] > 0)
    {
        eligibility[RECITE_UNHOLY] = 0;
    }

    // Anti-heretic prayer

    // Sleeping or paralyzed monsters will wake up or still perceive their
    // surroundings, respectively.  So, you can still recite to them.

    if (mons_intel(mon) >= I_NORMAL
        && !(mon->has_ench(ENCH_DUMB) || mons_is_confused(mon)))
    {
        // In the eyes of Zin, everyone is a sinner until proven otherwise!
        eligibility[RECITE_HERETIC]++;

        // Any priest is a heretic...
        if (mon->is_priest())
            eligibility[RECITE_HERETIC]++;

        // Or those who believe in themselves...
        if (mon->type == MONS_DEMIGOD)
            eligibility[RECITE_HERETIC]++;

        // ...but evil gods are worse.
        if (is_evil_god(mon->god) || is_unknown_god(mon->god))
            eligibility[RECITE_HERETIC]++;

        // (The above mean that worshipers will be treated as
        // priests for reciting, even if they aren't actually.)

        // Don't allow against enemies that are already affected by an
        // earlier recite type.
        if (eligibility[RECITE_CHAOTIC] > 0
            || eligibility[RECITE_IMPURE] > 0
            || eligibility[RECITE_UNHOLY] > 0)
        {
            eligibility[RECITE_HERETIC] = 0;
        }

        // Sanity check: monsters that won't attack you, and aren't
        // priests/evil, don't get recited against.
        if (mon->wont_attack() && eligibility[RECITE_HERETIC] <= 1)
            eligibility[RECITE_HERETIC] = 0;

        // Sanity check: monsters that are holy, know holy spells, or worship
        // holy gods aren't heretics.
        if (mon->is_holy() || is_good_god(mon->god))
            eligibility[RECITE_HERETIC] = 0;
    }

#ifdef DEBUG_DIAGNOSTICS
    string elig;
    for (int i = 0; i < NUM_RECITE_TYPES; i++)
        elig += '0' + eligibility[i];
    dprf("Eligibility: %s", elig.c_str());
#endif

    // Checking to see whether they were eligible for anything at all.
    for (int i = 0; i < NUM_RECITE_TYPES; i++)
        if (eligibility[i] > 0)
            return 1;

    return 0;
}

// Check whether there are monsters who might be influenced by Recite.
// If 'recite' is false, we're just checking whether we can.
// If it's true, we're actually reciting and need to present a menu.

// Returns 0, if no monsters found.
// Returns 1, if eligible audience found.
// Returns -1, if entire audience already affected or too dumb to understand.
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

    return true;
}

static const char* zin_book_desc[NUM_RECITE_TYPES] =
{
    "Chaotic (affects the forces of chaos and mutation)",
    "Impure (affects the unclean and corporeal undead)",
    "Heretic (affects non-believers)",
    "Unholy (affects demons and incorporeal undead)",
};

int zin_check_recite_to_monsters(recite_type *prayertype)
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
        switch (_zin_check_recite_to_single_monster(mon, retval))
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
        dprf("No audience found!");
        return 0;
    }
    else if (!found_eligible && found_ineligible)
    {
        dprf("No sensible audience found!");
        return -1;
    }

    if (!prayertype)
        return 1;

    int eligible_types = 0;
    for (int i = 0; i < NUM_RECITE_TYPES; i++)
        if (count[i] > 0)
            eligible_types++;

    // If there's only one eligible recitation, and we're actually reciting, then perform it.
    if (eligible_types == 1)
    {
        for (int i = 0; i < NUM_RECITE_TYPES; i++)
            if (count[i] > 0)
                *prayertype = (recite_type)i;

        return 1;
    }

    // But often, you'll have multiple options...
    mesclr();

    mprf(MSGCH_PROMPT, "Recite against which type of sinner?");

    int menu_cnt = 0;
    recite_type letters[NUM_RECITE_TYPES];

    for (int i = 0; i < NUM_RECITE_TYPES; i++)
    {
        if (count[i] > 0)
        {
            string affected_str;

            for (map<string, int>::iterator it = affected_by_type[i].begin();
                 it != affected_by_type[i].end(); it++)
            {
                if (!affected_str.empty())
                    affected_str += ", ";

                affected_str += it->first;
                if (it->second > 1)
                    affected_str += make_stringf(" x%d", it->second);
            }

            mprf("    [%c] - %s", 'a' + menu_cnt, zin_book_desc[i]);
            mprf("%d: %s", count[i], affected_str.c_str());
            letters[menu_cnt++] = (recite_type)i;
        }
    }
    flush_prev_message();

    while (true)
    {
        int keyn = toalower(getch_ck());

        if (keyn >= 'a' && keyn < 'a' + menu_cnt)
        {
            *prayertype = letters[keyn - 'a'];
            break;
        }
        else
            return 0;
    }

    return 1;
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

bool zin_recite_to_single_monster(const coord_def& where,
                                  recite_type prayertype)
{
    // That's a pretty good sanity check, I guess.
    if (!you_worship(GOD_ZIN))
        return false;

    monster* mon = monster_at(where);

    // Once you're already reciting, invis is ok.
    if (!mon || !cell_see_cell(where, you.pos(), LOS_DEFAULT))
        return false;

    recite_counts eligibility;
    bool affected = false;

    if (_zin_check_recite_to_single_monster(mon, eligibility) < 1)
        return false;

    // First check: are they even eligible for this kind of recitation?
    // (Monsters that have been hurt by recitation aren't eligible.)
    if (eligibility[prayertype] < 1)
        return false;

    // Second check: because this affects the whole screen over several turns,
    // its effects are staggered. There's a 50% chance per monster, per turn,
    // that nothing will happen - so the cumulative odds of nothing happening
    // are one in eight, since you recite three times.
    if (coinflip())
        return false;

    // Resistance is now based on HD. You can affect up to (30+30)/2 = 30 'power' (HD).
    int power = (skill_bump(SK_INVOCATIONS, 10) + you.piety * 3 / 2) / 20;
    // Old recite was mostly deterministic, which is bad.
    int resist = mon->get_experience_level() + random2(6);
    int check = power - resist;

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
                else if (mon->can_use_spells() && !mon->is_priest()
                         && !mons_class_flag(mon->type, M_FAKE_SPELLS))
                {
                    effect = ZIN_ANTIMAGIC;
                }
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
    const int hd = mon->get_experience_level();

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

    notify_stat_change(STAT_STR, stamina_amt, true, "");
    notify_stat_change(STAT_INT, stamina_amt, true, "");
    notify_stat_change(STAT_DEX, stamina_amt, true, "");

    return true;
}

void zin_remove_divine_stamina()
{
    mprf(MSGCH_DURATION, "Your divine stamina fades away.");
    notify_stat_change(STAT_STR, -you.attribute[ATTR_DIVINE_STAMINA],
                true, "Zin's divine stamina running out");
    notify_stat_change(STAT_INT, -you.attribute[ATTR_DIVINE_STAMINA],
                true, "Zin's divine stamina running out");
    notify_stat_change(STAT_DEX, -you.attribute[ATTR_DIVINE_STAMINA],
                true, "Zin's divine stamina running out");
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
    // (Checked in abl-show.cc.)
    if (env.sanctuary_time)
        return false;

    // Yes, shamelessly stolen from NetHack...
    if (!silenced(you.pos())) // How did you manage that?
        mprf(MSGCH_SOUND, "You hear a choir sing!");
    else
        mpr("You are suddenly bathed in radiance!");

    flash_view(WHITE);

    holy_word(100, HOLY_WORD_ZIN, you.pos(), true, &you);

#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    delay(1000);
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

    // shield bonus up to 8
    you.attribute[ATTR_DIVINE_SHIELD] = 3 + you.skill_rdiv(SK_SHIELDS, 1, 5);

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
    you.duration[DUR_RETCHING] = 0;
    you.duration[DUR_WEAK] = 0;
    restore_stat(STAT_ALL, 0, false);
    unrot_hp(9999);
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
        for (vector<coord_def>::iterator it = mimics.begin();
             it != mimics.end(); ++it)
        {
            discover_mimic(*it, false);
        }
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
    return you_worship(GOD_BEOGH) && !player_under_penance()
           && you.piety >= piety_breakpoint(4);
}

void jiyva_paralyse_jellies()
{
    mprf(MSGCH_PRAY, "You %s prayer to %s.",
         you.duration[DUR_JELLY_PRAYER] ? "renew your" : "offer a",
         god_name(you.religion).c_str());

    you.duration[DUR_JELLY_PRAYER] = 200;

    int jelly_count = 0;
    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        monster* mon = monster_at(*ri);

        if (mon != NULL && mons_is_slime(mon) && !mon->is_shapeshifter())
        {
            mon->add_ench(mon_enchant(ENCH_PARALYSIS, 0,
                                      &you, 200));
            jelly_count++;
        }
    }

    if (jelly_count > 0)
    {
        if (jelly_count > 1)
            mprf(MSGCH_PRAY, "The nearby slimes join your prayer.");
        else
            mprf(MSGCH_PRAY, "A nearby slime joins your prayer.");

        lose_piety(5);
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
    return you_worship(GOD_YREDELEMNUL) && !player_under_penance()
           && you.piety >= piety_breakpoint(1)
           && you.duration[DUR_MIRROR_DAMAGE];
}

bool yred_can_animate_dead()
{
    return you_worship(GOD_YREDELEMNUL) && !player_under_penance()
           && you.piety >= piety_breakpoint(2);
}

void yred_animate_remains_or_dead()
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
        }
    }
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

    // Drop the monster's holy equipment, and keep wielding the rest.
    monster_drop_things(mon, false, is_holy_item);

    const monster orig = *mon;

    // Use the original monster type as the zombified type here, to get
    // the proper stats from it.
    define_zombie(mon, mon->type, MONS_SPECTRAL_THING);

    // If the original monster has been drained or levelled up, its HD
    // might be different from its class HD, in which case its HP should
    // be rerolled to match.
    if (mon->hit_dice != orig.hit_dice)
    {
        mon->hit_dice = max(orig.hit_dice, 1);
        roll_zombie_hp(mon);
    }

    mon->colour = ETC_UNHOLY;

    mon->flags |= MF_NO_REWARD;
    mon->flags |= MF_ENSLAVED_SOUL;

    // If the original monster type has melee, spellcasting or priestly
    // abilities, make sure its spectral thing has them as well.
    mon->flags |= orig.flags & (MF_MELEE_MASK | MF_SPELL_MASK);
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

        mitm[index_of_corpse_created].props["never_hide"] = true;

        ASSERT(valid_corpse >= 0);

        // Higher piety means fresher corpses.  One out of ten corpses
        // will always be rotten.
        int rottedness = 200 -
            (!one_chance_in(10) ? random2(200 - you.piety)
                                : random2(100 + random2(75)));
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

bool kiku_take_corpse()
{
    for (int i = you.visible_igrd(you.pos()); i != NON_ITEM; i = mitm[i].link)
    {
        item_def &item(mitm[i]);

        if (item.base_type != OBJ_CORPSES || item.sub_type != CORPSE_BODY)
            continue;
        // should only fresh corpses count?

        // only nets currently, but let's check anyway...
        if (item_is_stationary(item))
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

bool fedhas_sunlight()
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
        return false;

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
    delay(200);

    if (revealed_count)
    {
        mprf("In the bright light, you notice %s.", revealed_count == 1 ?
             "an invisible shape" : "some invisible shapes");
    }

    return true;
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
        struct { short place; coord_def coord; int64_t game_start; } to_hash;
        to_hash.place = get_packed_place();
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
        if (keyin == '\n'  || keyin == '\r')
            return true;

        // Otherwise they should enter a digit
        if (isadigit(keyin))
        {
            selected = keyin - '0';
            if (selected > 0 && selected <= max)
                return true;
        }
        // else they entered some garbage?
    }

    return max;
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
        if (mons_class_can_pass(MONS_PLANT, env.grid(*adj_it))
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
        delay(200);
    }

    _decrease_amount(collected_fruit, created_count);

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

        if (ftype >= DNGN_MINMOVE)
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

// Destroy corpses in the player's LOS (first corpse on a stack only)
// and make 1 giant spore per corpse.  Spores are given the input as
// their starting behavior; the function returns the number of corpses
// processed.
int fedhas_corpse_spores(beh_type attitude, bool interactive)
{
    int count = 0;
    vector<stack_iterator> positions;

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
                positions.push_back(stack_it);
                count++;
                break;
            }
        }
    }

    if (count == 0)
        return count;

    viewwindow(false);
    for (unsigned i = 0; i < positions.size(); ++i)
    {
#ifndef USE_TILE_LOCAL
        coord_def temp = grid2view(positions[i]->pos);
        cgotoxy(temp.x, temp.y, GOTO_DNGN);

        unsigned color = GREEN | COLFLAG_FRIENDLY_MONSTER;
        color = real_colour(color);

        unsigned character = mons_char(MONS_GIANT_SPORE);
        put_colour_ch(color, character);
#endif
#ifdef USE_TILE
        tiles.add_overlay(positions[i]->pos, TILE_SPORE_OVERLAY);
#endif
    }

    if (interactive && yesnoquit("Will you create these spores?",
                                 true, 'y') <= 0)
    {
        viewwindow(false);
        return -1;
    }

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

bool fedhas_evolve_flora()
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
        mpr("No evolvable flora in sight.");
        return false;
    }

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
        return _place_ballisto(spelld.target);

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
        simple_monster_message(plant,
                               " can now pick up its mycelia and move.");
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

    plant->hit_dice += you.skill_rdiv(SK_INVOCATIONS);

    if (upgrade.fruit_cost)
        _decrease_amount(collected_fruit, upgrade.fruit_cost);

    if (upgrade.piety_cost)
    {
        lose_piety(upgrade.piety_cost);
        mpr("Your piety has decreased.");
    }

    return true;
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
    ouch(damage, NON_MONSTER, KILLED_BY_WILD_MAGIC, "a spatial distortion");
}

void cheibriados_time_bend(int pow)
{
    mpr("The flow of time bends around you.");

    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        monster* mon = monster_at(*ai);
        if (mon && !mon->is_stationary())
        {
            int res_margin = roll_dice(mon->hit_dice, 3) - random2avg(pow, 2);
            if (res_margin > 0)
            {
                mprf("%s%s",
                     mon->name(DESC_THE).c_str(),
                     mons_resist_string(mon, res_margin));
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
    if (stop_attack_prompt(hitfunc, "hurt", _act_slouchable))
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
    flash_view(LIGHTBLUE);
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
    delay(1000);
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

    flash_view(0);
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
    for (set<mid_t>::const_iterator wit = witnesses.begin();
         wit != witnesses.end(); ++wit)
    {
        monster *orc = monster_by_mid(*wit);
        if (!orc && !orc->alive())
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

/**
 * @file
 * @brief God-granted abilities.
**/

#include "AppHdr.h"

#include "godabil.h"

#include <cmath>
#include <numeric>
#include <queue>
#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "bloodspatter.h"
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
#include "english.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "format.h" // formatted_string
#include "godblessing.h"
#include "godcompanions.h"
#include "goditem.h"
#include "godpassive.h"
#include "hints.h"
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
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-death.h"
#include "mon-gear.h" // H: give_weapon()/give_armour()
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "mon-util.h"
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
#include "rot.h"
#include "shout.h"
#include "skill_menu.h"
#include "spl-book.h"
#include "spl-goditem.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-wpnench.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "sprint.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h" // monster_teleport
#include "terrain.h"
#ifdef USE_TILE
 #include "tiledef-main.h"
#endif
#include "timed_effects.h"
#include "traps.h"
#include "viewchar.h"
#include "view.h"

static bool _player_sacrificed_arcana();

// Load the sacrifice_def definition and the sac_data array.
#include "sacrifice-data.h"

/** Would a god currently allow using a one-time six-star ability?
 * Does not check whether the god actually grants such an ability.
 */
bool can_do_capstone_ability(god_type god)
{
   return in_good_standing(god, 5) && !you.one_time_ability_used[god];
}

static const char *_god_blessing_description(god_type god)
{
    switch (god)
    {
    case GOD_SHINING_ONE:
        return "blessed by the Shining One";
    case GOD_LUGONU:
        return "corrupted by Lugonu";
    case GOD_KIKUBAAQUDGHA:
        return "bloodied by Kikubaaqudgha";
    default:
        return "touched by the gods";
    }
}

/**
 * Perform a capstone god ability that blesses a weapon with the god's
 * brand.

 * @param god    The god performing the blessing.
 * @param brand  The brand being granted.
 * @param colour The colour to flash when the weapon is branded.
 * @returns True if the weapon was successfully branded, false otherwise.
*/
bool bless_weapon(god_type god, brand_type brand, colour_t colour)
{
    ASSERT(can_do_capstone_ability(god));

    int item_slot = prompt_invent_item("Brand which weapon?", MT_INVLIST,
                                       OSEL_BLESSABLE_WEAPON, true, true,
                                       false);

    if (item_slot == PROMPT_NOTHING || item_slot == PROMPT_ABORT)
    {
        canned_msg(MSG_OK);
        return false;
    }

    item_def& wpn(you.inv[item_slot]);
    // Only TSO allows blessing ranged weapons.
    if (!is_brandable_weapon(wpn, brand == SPWPN_HOLY_WRATH, true))
        return false;

    string prompt = "Do you wish to have " + wpn.name(DESC_YOUR)
                       + " ";
    if (brand == SPWPN_PAIN)
        prompt += "bloodied with pain";
    else if (brand == SPWPN_DISTORTION)
        prompt += "corrupted with distortion";
    else
        prompt += "blessed with holy wrath";
    prompt += "?";
    if (!yesno(prompt.c_str(), true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (you.duration[DUR_EXCRUCIATING_WOUNDS]) // just in case
    {
        ASSERT(you.weapon());
        end_weapon_brand(*you.weapon());
    }

    string old_name = wpn.name(DESC_A);
    set_equip_desc(wpn, ISFLAG_GLOWING);
    set_item_ego_type(wpn, OBJ_WEAPONS, brand);
    enchant_weapon(wpn, true);
    enchant_weapon(wpn, true);
    if (wpn.cursed())
        do_uncurse_item(wpn);

    if (god == GOD_SHINING_ONE)
    {
        convert2good(wpn);

        if (is_blessed_convertible(wpn))
            origin_acquired(wpn, GOD_SHINING_ONE);
    }
    else if (is_evil_god(god))
        convert2bad(wpn);

    you.wield_change = true;
    you.one_time_ability_used.set(god);
    calc_mp(); // in case the old brand was antimagic,
    you.redraw_armour_class = true; // protection,
    you.redraw_evasion = true;      // or evasion
    const string desc = old_name + " " + _god_blessing_description(god);
    take_note(Note(NOTE_ID_ITEM, 0, 0, wpn.name(DESC_A), desc));
    wpn.flags |= ISFLAG_NOTED_ID;
    wpn.props[FORCED_ITEM_COLOUR_KEY] = colour;

    mprf(MSGCH_GOD, "Your %s shines brightly!", wpn.name(DESC_QUALNAME).c_str());
    flash_view(UA_PLAYER, colour);
    simple_god_message(" booms: Use this gift wisely!");
    you.one_time_ability_used.set(you.religion);
    take_note(Note(NOTE_GOD_GIFT, you.religion));

    if (god == GOD_SHINING_ONE)
    {
        holy_word(100, HOLY_WORD_TSO, you.pos(), true);
        // Un-bloodify surrounding squares.
        for (radius_iterator ri(you.pos(), 3, C_SQUARE, LOS_SOLID); ri; ++ri)
            if (is_bloodcovered(*ri))
                env.pgrid(*ri) &= ~FPROP_BLOODY;
    }
    else if (god == GOD_KIKUBAAQUDGHA)
    {
        you.gift_timeout = 1; // no protection during pain branding weapon
        torment(&you, TORMENT_KIKUBAAQUDGHA, you.pos());
        you.gift_timeout = 0; // protection after pain branding weapon
        // Bloodify surrounding squares (75% chance).
        for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_SOLID); ri; ++ri)
            if (!one_chance_in(4))
                maybe_bloodify_square(*ri);
    }

#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    scaled_delay(1000);
#endif
    return true;
}

static int _gold_to_donation(int gold)
{
    return static_cast<int>((gold * log((float)gold)) / MAX_PIETY);
}

// donate gold to gain piety distributed over time
bool zin_donate_gold()
{
    if (you.gold == 0)
    {
        mpr("You don't have anything to sacrifice.");
        return false;
    }

    if (!yesno("Do you wish to donate half of your money?", true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    const int donation_cost = (you.gold / 2) + 1;
    const int donation = _gold_to_donation(donation_cost);

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE) || defined(DEBUG_PIETY)
    mprf(MSGCH_DIAGNOSTICS, "A donation of $%d amounts to an "
         "increase of piety by %d.", donation_cost, donation);
#endif
    // Take a note of the donation.
    take_note(Note(NOTE_DONATE_MONEY, donation_cost));

    you.attribute[ATTR_DONATIONS] += donation_cost;

    you.del_gold(donation_cost);

    if (donation < 1)
    {
        simple_god_message(" finds your generosity lacking.");
        return false;
    }

    you.duration[DUR_PIETY_POOL] += donation;
    if (you.duration[DUR_PIETY_POOL] > 30000)
        you.duration[DUR_PIETY_POOL] = 30000;

    const int estimated_piety =
        min(MAX_PENANCE + MAX_PIETY, you.piety + you.duration[DUR_PIETY_POOL]);

    if (player_under_penance())
    {
        if (estimated_piety >= you.penance[GOD_ZIN])
            mpr("You feel that you will soon be absolved of all your sins.");
        else
            mpr("You feel that your burden of sins will soon be lighter.");
    }
    else
    {
        string result = "You feel that " + god_name(GOD_ZIN) + " will soon be ";
        result +=
            (estimated_piety >= piety_breakpoint(5)) ? "exalted by your worship" :
            (estimated_piety >= piety_breakpoint(4)) ? "extremely pleased with you" :
            (estimated_piety >= piety_breakpoint(3)) ? "greatly pleased with you" :
            (estimated_piety >= piety_breakpoint(2)) ? "most pleased with you" :
            (estimated_piety >= piety_breakpoint(1)) ? "pleased with you" :
            (estimated_piety >= piety_breakpoint(0)) ? "aware of your devotion"
                                                     : "noncommittal";
        result += (donation >= 30 && you.piety < piety_breakpoint(5)) ? "!" : ".";

        mpr(result);
    }

    return true;
}

static void _zin_saltify(monster* mon);

static const char * const book_of_zin[][3] =
{
    {
        "It was the word of Zin that there would not be @sin_noun@...",
        "...and did the people not suffer until they had @smitten@...",
        "...the @sinners@, after which all was well?",
    },

    {
        "The voice of Zin, pure and clear, did say that the @sinners@...",
        "...were not @virtuous@! And hearing this, the people rose up...",
        "...and embraced @virtue@, for they feared Zin's wrath.",
    },

    {
        "Zin spoke of the doctrine of @virtue@, and...",
        "...saw the @sinners@ filled with fear, for they were...",
        "...@sin_adj@ and knew Zin's wrath would come for them.",
    },

    {
        "And so Zin bade the @sinners@ to come before...",
        "...the altar, that judgement might be passed...",
        "...upon those who were not @virtuous@.",
    },

    {
        "To the devout, Zin provideth. From the rest...",
        "...ye @sinners@, ye guilty...",
        "...of @sin_noun@, Zin taketh.",
    },

    {
        "Zin saw the @sin_noun@ of the @sinners@, and...",
        "...was displeased, for did the law not say that...",
        "...those who did not become @virtuous@ would be @smitten@?",
    },

    {
        "Zin said that @virtue@ shall be the law of the land, and...",
        "...those who turn to @sin_noun@ will be @smitten@. This was fair...",
        "...and just, and not a voice dissented.",
    },

    {
        "Damned, damned be the @sinners@ and...",
        "...all else who abandon @virtue@! Let them...",
        "...be @smitten@ by the jurisprudence of Zin!",
    },

    {

        "And Zin said to all in attendance, 'Which of ye...",
        "...number among the @sinners@? Come before me, that...",
        "...I may @smite@ you now for your @sin_noun@!'",
    },

    {
        "Yea, I say unto thee, bring forth...",
        "...the @sinners@ that they may know...",
        "...the wrath of Zin, and thus be @smitten@!",
    },

    {
        "In a great set of silver scales are weighed the...",
        "...souls of the @sinners@, and with their @sin_adj@...",
        "...ways, the balance hath tipped against them!",
    },

    {
        "It is just that the @sinners@ shall be @smitten@...",
        "...in due time, for @virtue@ is what Zin has declared...",
        "...the law of the land, and Zin's word is law!",
    },

    {
        "Thus the people made the covenant of @virtue@ with...",
        "...Zin, and all was good, for they knew that the...",
        "...@sinners@ would trouble them no longer.",
    },

    {
        "What of the @sinners@? @Smitten@ for their...",
        "...@sin_noun@ they shall be! Zin will @smite@ them again...",
        "...and again, and again!",
    },

    {
        "And lo, the wrath of Zin did find...",
        "...them wherever they hid, and the @sinners@...",
        "...were @smitten@ for their @sin_noun@!",
    },

    {
        "Zin looked out upon the remains of the @sinners@...",
        "...and declared it good that they had been...",
        "...@smitten@. And thus justice was done.",
    },

    {
        "The law of Zin demands thee...",
        "...be @virtuous@, and that the punishment for @sin_noun@...",
        "...shall be swift and harsh!",
    },

    {
        "It was then that Zin bade them...",
        "...not to stray from @virtue@, lest...",
        "...they become as damned as the @sinners@.",
    },

    {
        "Only the @virtuous@ shall be judged worthy, and...",
        "...all the @sinners@ will be found wanting. Such is...",
        "...the word of Zin, and such is the law!",
    },

    {
        "To those who would swear an oath of @virtue@ on my altar...",
        "...I bring ye salvation. To the rest, ye @sinners@...",
        "...and the @sin_adj@, the name of Zin shall be thy damnation.",
    },

    {
        "And Zin decreed that the people would be...",
        "...protected from @sin_noun@ in all its forms, and...",
        "...preserved in their @virtue@ for all the days to come.",
    },

    {
        "For those who would enter Zin's holy bosom...",
        "...and live in @virtue@, Zin provideth. Such is...",
        "...the covenant, and such is the way of things.",
    },

    {
        "Zin hath not damned the @sinners@, but it is they...",
        "...that have damned themselves for their @sin_noun@, for...",
        "...did Zin not decree that to be @sin_adj@ was wrong?",
    },

    {
        "And Zin, furious at their @sin_noun@, held...",
        "...aloft a silver sceptre! The @sinners@...",
        "...were @smitten@, and thus the way of things was maintained.",
    },

    {
        "When the law of the land faltered, Zin rose...",
        "...from the silver throne, and the @sinners@ were...",
        "...@smitten@. And it was thus that the law was made good.",
    },

    {
        "Zin descended from on high in a silver chariot...",
        "...to @smite@ the @sinners@ for their...",
        "...@sin_noun@, and thus judgement was rendered.",
    },

    {
        "The @sinners@ stood before Zin, and in that instant...",
        "...they knew they would be found guilty of @sin_noun@...",
        "...for that is the word of Zin, and Zin's word is law.",
    },
};

static const char * const sinner_text[] =
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

// First column is adjective, then noun.
static const char * const sin_text[][2] =
{
    { "chaotic",      "chaos" },
    { "discordant",   "discord" },
    { "anarchic",     "anarchy" },
    { "unclean",      "uncleanliness" },
    { "impure",       "impurity" },
    { "contaminated", "contamination" },
    { "unfaithful",   "unfaithfulness" },
    { "disloyal",     "disloyalty" },
    { "doubting",     "doubt" },
    { "profane",      "profanity" },
    { "blasphemous",  "blasphemy" },
    { "sacrilegious", "sacrilege" },
};

// First column is adjective, then noun.
static const char * const virtue_text[][2] =
{
    { "ordered",   "order" },
    { "harmonic",  "harmony" },
    { "lawful",    "lawfulness" },
    { "clean",     "cleanliness" },
    { "pure",      "purity" },
    { "hygienic",  "hygiene" },
    { "faithful",  "faithfulness" },
    { "loyal",     "loyalty" },
    { "believing", "belief" },
    { "reverent",  "reverence" },
    { "pious",     "piety" },
    { "obedient",  "obedience" },
};

// First column is infinitive, then gerund.
static const char * const smite_text[][2] =
{
    { "purify",      "purified" },
    { "censure",     "censured" },
    { "condemn",     "condemned" },
    { "strike down", "struck down" },
    { "expel",       "expelled" },
    { "oust",        "ousted" },
    { "smite",       "smitten" },
    { "castigate",   "castigated" },
    { "rebuke",      "rebuked" },
};

/** Get the verse to recite this turn.
 *
 *  @param seed       The seed to keep the book coherent between turns.
 *  @param prayertype One of the four recite types.
 *  @param step       -1: We're either starting or stopping, so we just want the passage name.
 *                    2/1/0: That many rounds are left. So, if step = 2, we want to show the passage #1/3.
 *  @returns the verse to be said this turn, or if step == -1, which verse it is.
 */
string zin_recite_text(const int seed, const int prayertype, int step)
{
    // To have deterministic passages we extract portions of the seed.
    // We use trits: "digits" in the base-3 expansion of seed.

    COMPILE_CHECK(ARRAYSZ(book_of_zin) == 27);
    const int chapter = seed % 27;
    const int verse = (seed/27) % 81;

    // Change step to turn 1, turn 2, or turn 3.
    if (step > -1)
    {
        step = abs(step-3);
        if (step > 3)
            step = 1;
    }
    else
    {
        const string bookname = (prayertype == RECITE_CHAOTIC)  ?  "Abominations" :
                                (prayertype == RECITE_IMPURE)   ?  "Ablutions"    :
                                (prayertype == RECITE_HERETIC)  ?  "Apostates"    :
                                (prayertype == RECITE_UNHOLY)   ?  "Anathema"     :
                                                                   "Bugginess";
        ostringstream numbers;
        numbers << (chapter + 1);
        numbers << ":";
        numbers << (verse + 1);
        return bookname + " " + numbers.str();
    }

    // These mad-libs are deterministically derived from the verse number
    // and prayer type. Sins and virtues are paired, so use the same sub-
    // seed. Sinners, sins, and smites are uncorrelated so do not share
    // trits.
    COMPILE_CHECK(ARRAYSZ(sinner_text) == 12);
    COMPILE_CHECK(ARRAYSZ(sin_text) == 12);
    COMPILE_CHECK(ARRAYSZ(virtue_text) == 12);
    const int sinner_seed = verse % 3 + prayertype * 3;
    const int sin_seed = (verse/27) % 3 + prayertype * 3;
    const int virtue_seed = sin_seed;

    COMPILE_CHECK(ARRAYSZ(smite_text) == 9);
    const int smite_seed = (verse/3) % 9;

    string recite = book_of_zin[chapter][step-1];

    const map<string, string> replacements =
    {
        { "sinners", sinner_text[sinner_seed] },

        { "sin_adj",  sin_text[sin_seed][0] },
        { "sin_noun", sin_text[sin_seed][1] },

        { "virtuous", virtue_text[virtue_seed][0] },
        { "virtue",   virtue_text[virtue_seed][1] },

        { "smite",   smite_text[smite_seed][0] },
        { "smitten", smite_text[smite_seed][1] },
        { "Smitten", uppercase_first(smite_text[smite_seed][1]) },
    };

    return replace_keys(recite, replacements);
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
    // surroundings, respectively. So, you can still recite to them.
    if (mons_intel(*mon) >= I_HUMAN
        && !(mon->has_ench(ENCH_DUMB) || mons_is_confused(*mon)))
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

/** Check whether this monster might be influenced by Recite.
 *
 * @param[in] mon The monster to check.
 * @param[out] eligibility A vector, indexed by recite_type, that indicates
 *             which recitation types the monster is affected by, if any:
 *             eligibility[RECITE_FOO] is nonzero if the monster is affected
 *             by RECITE_FOO.
 * @param quiet[in]     Whether to suppress messaging.
 * @returns Whether the monster is eligible for recite. If the monster can be
 *          recited to, the eligibility vector indicates the valid types of
 *          recite.
 */
recite_eligibility zin_check_recite_to_single_monster(const monster *mon,
                                                  recite_counts &eligibility,
                                                  bool quiet)
{
    ASSERT(mon);

    // Can't recite anyway if they were recently recited to.
    if (mon->has_ench(ENCH_RECITE_TIMER))
        return RE_RECITE_TIMER;

    const mon_holy_type holiness = mon->holiness();
    eligibility.init(0);

    // Anti-chaos prayer: Hits things vulnerable to silver, or with chaotic spells/gods.
    eligibility[RECITE_CHAOTIC] = mon->how_chaotic(true);

    // Anti-impure prayer: Hits things that Zin hates in general.
    // Don't look at the monster's god; that's what RECITE_HERETIC is for.
    eligibility[RECITE_IMPURE] = mon->how_unclean(false);

    // Anti-unholy prayer: Hits demons and incorporeal undead.
    if (holiness & MH_UNDEAD && mon->is_insubstantial()
        || holiness & MH_DEMONIC)
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

    bool maybe_eligible = false;
    // Checking to see whether they were eligible for anything at all.
    for (int i = 0; i < NUM_RECITE_TYPES; i++)
        if (eligibility[i] > 0)
            maybe_eligible = true;

    if (maybe_eligible)
    {
        // Too high HD to be affected at current power.
        if (mon->get_hit_dice() >= zin_recite_power())
            return RE_TOO_STRONG;
        return RE_ELIGIBLE;
    }

    return RE_INELIGIBLE;
}

int zin_recite_power()
{
    // Resistance is now based on HD.
    // You can affect up to (30+30)/2 = 30 'power' (HD).
    const int power_mult = 10;
    const int invo_power = you.skill_rdiv(SK_INVOCATIONS, power_mult)
                           + 3 * power_mult;
    const int piety_power = you.piety * 3 / 2;
    return (invo_power + piety_power) / 2 / power_mult;
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
 * @return  0 if no eligible monsters were found.
 * @return  1 if an eligible audience was found.
 * @return  -1 if the only monsters found cannot currently be affected (either
 *          due to lack of recite power, or already having been affected)
 *
 */
int zin_check_recite_to_monsters(bool quiet)
{
    bool found_temp_ineligible = false;
    bool found_eligible = false;

    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        const monster *mon = monster_at(*ri);
        if (!mon || !you.can_see(*mon))
            continue;

        recite_counts retval;
        switch (zin_check_recite_to_single_monster(mon, retval, quiet))
        {
        case RE_TOO_STRONG:
        case RE_RECITE_TIMER:
            found_temp_ineligible = true;
        // Intentional fallthrough
        case RE_INELIGIBLE:
            continue;

        case RE_ELIGIBLE:
            found_eligible = true;
        }
    }

    if (!found_eligible && !found_temp_ineligible)
    {
        if (!quiet)
            dprf("No audience found!");
        return 0;
    }
    else if (!found_eligible && found_temp_ineligible)
    {
        if (!quiet)
            dprf("No sensible audience found!");
        return -1;
    }
    else
        return 1; // We just recite against everything.
}

enum zin_eff
{
    ZIN_NOTHING,
    ZIN_DAZE,
    ZIN_CONFUSE,
    ZIN_PARALYSE,
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

    if (zin_check_recite_to_single_monster(mon, eligibility) != RE_ELIGIBLE)
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

    const int power = zin_recite_power();
    // Old recite was mostly deterministic, which is bad.
    const int resist = mon->get_hit_dice() + random2(6);
    const int check = power - resist;

    // We abort if we didn't *beat* their HD - but first we might get a cute message.
    if (mon->can_speak() && one_chance_in(5))
    {
        if (check < -10)
            simple_monster_message(*mon, " guffaws at your puny god.");
        else if (check < -5)
            simple_monster_message(*mon, " sneers at your recitation.");
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
                    effect = ZIN_CONFUSE;
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
            if (coinflip() && mon->res_rotting() <= 1)
                effect = ZIN_ROT;
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
        // because this is divine punishment. Those with no real flesh are
        // immune, of course.
        if (check < 5)
        {
            if (coinflip() && mon->res_rotting() <= 1)
                effect = ZIN_ROT;
            else
                effect = ZIN_SMITE;
        }
        else if (check < 10)
        {
            if (coinflip())
                effect = ZIN_SMITE;
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
            if (coinflip())
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
            simple_monster_message(*mon, " is dazed by your recitation.");
            affected = true;
        }
        break;

    case ZIN_CONFUSE:
        if (!mon->check_clarity(false)
            && mon->add_ench(mon_enchant(ENCH_CONFUSION, degree, &you,
                             (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            if (prayertype == RECITE_HERETIC)
                simple_monster_message(*mon, " is confused by your recitation.");
            else
                simple_monster_message(*mon, " stumbles about in disarray.");
            affected = true;
        }
        break;

    case ZIN_PARALYSE:
        if (mon->add_ench(mon_enchant(ENCH_PARALYSIS, 0, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(*mon,
                minor ? " is awed by your recitation."
                      : " is aghast at the heresy of your recitation.");
            affected = true;
        }
        break;

    case ZIN_SMITE:
        if (minor)
            simple_monster_message(*mon, " is smitten by the wrath of Zin.");
        else
            simple_monster_message(*mon, " is blasted by the fury of Zin!");
        // XXX: This duplicates code in cast_smiting().
        mon->hurt(&you, 7 + (random2(spellpower) * 33 / 191));
        if (mon->alive())
            print_wounds(*mon);
        affected = true;
        break;

    case ZIN_BLIND:
        if (mon->add_ench(mon_enchant(ENCH_BLIND, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(*mon, " is struck blind by the wrath of Zin!");
            affected = true;
        }
        break;

    case ZIN_SILVER_CORONA:
        if (mon->add_ench(mon_enchant(ENCH_SILVER_CORONA, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(*mon, " is limned with silver light.");
            affected = true;
        }
        break;

    case ZIN_ANTIMAGIC:
        ASSERT(prayertype == RECITE_HERETIC);
        if (mon->add_ench(mon_enchant(ENCH_ANTIMAGIC, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(*mon,
                minor ? " quails at your recitation."
                      : " looks feeble and powerless before your recitation.");
            affected = true;
        }
        break;

    case ZIN_MUTE:
        if (mon->add_ench(mon_enchant(ENCH_MUTE, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(*mon, " is struck mute by the wrath of Zin!");
            affected = true;
        }
        break;

    case ZIN_MAD:
        if (mon->add_ench(mon_enchant(ENCH_MAD, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(*mon, " is driven mad by the wrath of Zin!");
            affected = true;
        }
        break;

    case ZIN_DUMB:
        if (mon->add_ench(mon_enchant(ENCH_DUMB, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(*mon, " is left stupefied by the wrath of Zin!");
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
                mon->hurt(&you, damage, BEAM_MISSILE, KILLED_BY_BEAM,
                          "", "", false);

                if (mon->alive())
                {
                    simple_monster_message(*mon,
                      (damage < 25) ? "'s chaotic flesh sizzles and spatters!" :
                      (damage < 50) ? "'s chaotic flesh bubbles and boils."
                                    : "'s chaotic flesh runs like molten wax.");

                    print_wounds(*mon);
                    behaviour_event(mon, ME_WHACK, &you);
                    affected = true;
                }
                else
                {
                    simple_monster_message(*mon,
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
        // FIXME: no message (other than "You kill X!") is produced if the
        // rotting kills the monster.
        if (mon->res_rotting() <= 1
            && mon->rot(&you, 1 + roll_dice(2, degree), true))
        {
            mon->add_ench(mon_enchant(ENCH_SICK, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY));
            switch (prayertype)
            {
            case RECITE_CHAOTIC:
                simple_monster_message(*mon,
                    minor ? "'s chaotic flesh is covered in bleeding sores."
                          : "'s chaotic flesh erupts into weeping sores!");
                break;
            case RECITE_IMPURE:
                simple_monster_message(*mon,
                    minor ? "'s impure flesh rots away."
                          : "'s impure flesh sloughs off!");
                break;

            default:
                die("bad recite rot");
            }
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
        && mons_can_shout(mon->type))
    {
        monster_attempt_shout(*mon);
    }

    return true;
}

static void _zin_saltify(monster* mon)
{
    const coord_def where = mon->pos();
    const monster_type pillar_type =
        mons_is_zombified(*mon) ? mons_zombie_base(*mon)
                                : mons_species(mon->type);
    const int hd = mon->get_hit_dice();

    simple_monster_message(*mon, " is turned into a pillar of salt by the wrath of Zin!");

    // If the monster leaves a corpse when it dies, destroy the corpse.
    item_def* corpse = monster_die(mon, KILL_YOU, NON_MONSTER);
    if (corpse)
        destroy_item(corpse->index());

    if (monster *pillar = create_monster(
                        mgen_data(MONS_PILLAR_OF_SALT,
                                  BEH_HOSTILE,
                                  where,
                                  MHITNOT,
                                  MG_FORCE_PLACE).set_base(pillar_type),
                                  false))
    {
        // Enemies with more HD leave longer-lasting pillars of salt.
        int time_left = (random2(8) + hd) * BASELINE_DELAY;
        mon_enchant temp_en(ENCH_SLOWLY_DYING, 1, 0, time_left);
        pillar->update_ench(temp_en);
    }
}

bool zin_vitalisation()
{
    simple_god_message(" grants you divine stamina.");

    // Add divine stamina.
    const int stamina_amt =
        max(1, you.skill_rdiv(SK_INVOCATIONS, 1, 3));
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
    ASSERT(how_mutated());
    ASSERT(can_do_capstone_ability(you.religion));

    if (!yesno("Do you wish to cure all of your mutations?", true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }
    flash_view(UA_PLAYER, WHITE);
#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    scaled_delay(1000);
#endif

    you.one_time_ability_used.set(GOD_ZIN);
    take_note(Note(NOTE_GOD_GIFT, you.religion));
    simple_god_message(" draws all chaos from your body!");
    delete_all_mutations("Zin's power");
    return true;
}

void zin_sanctuary()
{
    ASSERT(!env.sanctuary_time);

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

}

// shield bonus = attribute for duration turns, then decreasing by 1
//                every two out of three turns
// overall shield duration = duration + attribute
// recasting simply resets those two values (to better values, presumably)
void tso_divine_shield()
{
    if (!you.duration[DUR_DIVINE_SHIELD])
    {
        if (you.shield())
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
    you.attribute[ATTR_DIVINE_SHIELD] =
        3 + you.skill_rdiv(SK_INVOCATIONS, 1, 5);

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
    // propelling it towards the victim. This is the most popular way, but
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
        || spell == SPELL_VIOLENT_UNRAVELLING
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

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        cloud_struct* cloud = cloud_at(*ri);
        int count = 0;
        for (stack_iterator si(*ri); si; ++si)
        {
            if (!item_is_spellbook(*si))
                continue;

            // If a grid is blocked, books lying there will be ignored.
            // Allow bombing of monsters.
            if (cell_is_solid(*ri)
                || cloud && cloud->type != CLOUD_FIRE)
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
            destroy_spellbook(*si);
            item_was_destroyed(*si);
            destroy_item(si.index());
            count++;
        }

        if (count)
        {
            if (cloud)
            {
                // Reinforce the cloud.
                mpr("The fire roars with new energy!");
                const int extra_dur = count + random2(6);
                cloud->decay += extra_dur * 5;
                cloud->set_whose(KC_YOU);
                continue;
            }

            const int duration = min(4 + count + random2(6), 20);
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
        return !mimics.empty();
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

/**
 * Has the monster been given a Beogh gift?
 *
 * @param mon the orc in question.
 * @returns whether you have given the monster a Beogh gift before now.
 */
static bool _given_gift(const monster* mon)
{
    return mon->props.exists(BEOGH_RANGE_WPN_GIFT_KEY)
            || mon->props.exists(BEOGH_MELEE_WPN_GIFT_KEY)
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

    if (!is_orcish_follower(*mons) || mons_genus(mons->type) != MONS_ORC)
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
    args.self = CONFIRM_CANCEL;
    args.show_floor_desc = true;
    args.top_prompt = "Select a follower to give a gift to.";

    direction(spd, args);

    if (!spd.isValid)
        return false;

    monster* mons = monster_at(spd.target);
    if (!beogh_can_gift_items_to(mons, false))
        return false;

    int item_slot = prompt_invent_item("Give which item?",
                                       MT_INVLIST, OSEL_BEOGH_GIFT, true);

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
    const item_def* mons_alt_weapon = mons->mslot_item(MSLOT_ALT_WEAPON);

    if (weapon && !mons->could_wield(gift)
        || body_armour && !check_armour_size(gift, mons->body_size())
        || !item_is_selected(gift, OSEL_BEOGH_GIFT))
    {
        mprf("You can't give that to %s.", mons->name(DESC_THE, false).c_str());

        return false;
    }
    else if (shield
             && (mons_weapon && mons->hands_reqd(*mons_weapon) == HANDS_TWO
                 || mons_alt_weapon
                    && mons->hands_reqd(*mons_alt_weapon) == HANDS_TWO))
    {
        mprf("%s can't equip that with a two-handed weapon.",
             mons->name(DESC_THE, false).c_str());
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
        mons->swap_weapons();

    dprf("is_ranged weap: %d", range_weapon);
    if (range_weapon)
        gift_ammo_to_orc(mons, true); // give a small initial ammo freebie


    if (shield)
        mons->props[BEOGH_SH_GIFT_KEY] = true;
    else if (body_armour)
        mons->props[BEOGH_ARM_GIFT_KEY] = true;
    else if (range_weapon)
        mons->props[BEOGH_RANGE_WPN_GIFT_KEY] = true;
    else
        mons->props[BEOGH_MELEE_WPN_GIFT_KEY] = true;

    return true;
}

bool beogh_resurrect()
{
    item_def* corpse = nullptr;
    bool found_any = false;
    for (stack_iterator si(you.pos()); si; ++si)
        if (si->props.exists(ORC_CORPSE_KEY))
        {
            found_any = true;
            if (yesno(("Resurrect "
                       + si->props[ORC_CORPSE_KEY].get_monster().name(DESC_THE)
                       + "?").c_str(), true, 'n'))
            {
                corpse = &*si;
                break;
            }
        }
    if (!corpse)
    {
        mprf("There's nobody %shere you can resurrect.",
             found_any ? "else " : "");
        return false;
    }

    coord_def pos;
    ASSERT(corpse->props.exists(ORC_CORPSE_KEY));
    for (fair_adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (!actor_at(*ai)
            && corpse->props[ORC_CORPSE_KEY].get_monster().is_location_safe(*ai))
        {
            pos = *ai;
        }
    }
    if (pos.origin())
    {
        mpr("There's no room!");
        return false;
    }

    monster* mon = get_free_monster();
    *mon = corpse->props[ORC_CORPSE_KEY];
    destroy_item(corpse->index());
    env.mid_cache[mon->mid] = mon->mindex();
    mon->hit_points = mon->max_hit_points;
    mon->inv.init(NON_ITEM);
    for (stack_iterator si(you.pos()); si; ++si)
    {
        if (!si->props.exists(DROPPER_MID_KEY)
            || si->props[DROPPER_MID_KEY].get_int() != int(mon->mid))
        {
            continue;
        }
        unwind_var<int> save_speedinc(mon->speed_increment);
        mon->pickup_item(*si, false, true);
    }
    mon->move_to_pos(pos);
    mon->timeout_enchantments(100);
    beogh_convert_orc(mon, conv_t::RESURRECTION);

    return true;
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

void yred_make_enslaved_soul(monster* mon, bool force_hostile)
{
    ASSERT(mon); // XXX: change to monster &mon
    ASSERT(mons_enslaved_body_and_soul(*mon));

    add_daction(DACT_OLD_ENSLAVED_SOULS_POOF);
    remove_enslaved_soul_companion();

    const string whose = you.can_see(*mon) ? apostrophise(mon->name(DESC_THE))
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

    // Rebrand or drop any holy equipment, and keep wielding the rest. Also
    // remove any active avatars.
    item_def *wpn = mon->mslot_item(MSLOT_WEAPON);
    if (wpn && get_weapon_brand(*wpn) == SPWPN_HOLY_WRATH)
    {
        set_item_ego_type(*wpn, OBJ_WEAPONS, SPWPN_DRAINING);
        convert2bad(*wpn);
    }
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
    monster_spells spl = orig.spells;
    for (const mon_spell_slot &slot : spl)
        if (!(get_spell_flags(slot.spell) & SPFLAG_HOLY))
            mon->spells.push_back(slot);
    if (mon->spells.size())
        mon->props[CUSTOM_SPELLS_KEY] = true;

    name_zombie(*mon, orig);

    mons_make_god_gift(*mon, GOD_YREDELEMNUL);
    add_companion(mon);

    mon->attitude = !force_hostile ? ATT_FRIENDLY : ATT_HOSTILE;
    behaviour_event(mon, ME_ALERT, force_hostile ? &you : 0);

    mon->stop_constricting_all(false);
    mon->stop_being_constricted();

    if (orig.halo_radius()
        || orig.umbra_radius()
        || orig.silence_radius())
    {
        invalidate_agrid();
    }

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
    for (radius_iterator ri(you.pos(), corpse_delivery_radius, C_SQUARE,
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

    for (radius_iterator ri(you.pos(), corpse_delivery_radius, C_SQUARE,
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
        const int adjusted_power = min(pow / 4, random2(random2(pow)));
        // Pick a place based on the power. This may be below the branch's
        // start, that's ok.
        const level_id lev(you.where_are_you, adjusted_power
                           - absdungeon_depth(you.where_are_you, 0));
        const monster_type mon_type = pick_local_corpsey_monster(lev);
        ASSERT(mons_class_can_be_zombified(mons_species(mon_type)));

        // Create corpse object.
        monster dummy;
        dummy.type = mon_type;
        define_monster(dummy);
        dummy.position = *ri;

        item_def* corpse = place_monster_corpse(dummy, true, true);
        if (!corpse)
            continue;

        // Higher piety means fresher corpses.
        int rottedness = 200 -
            (!one_chance_in(10) ? random2(200 - you.piety)
                                : random2(100 + random2(75)));
        corpse->freshness = rottedness;
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

bool kiku_gift_necronomicon()
{
    ASSERT(can_do_capstone_ability(you.religion));

    if (!yesno("Do you wish to receive a Necronomicon?", true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }
    int thing_created = items(true, OBJ_BOOKS, BOOK_NECRONOMICON, 1, 0,
                              you.religion);
    if (thing_created == NON_ITEM
        || !move_item_to_grid(&thing_created, you.pos()))
    {
        return false;
    }
    set_ident_type(mitm[thing_created], true);
    simple_god_message(" grants you a gift!");
    flash_view(UA_PLAYER, RED);
#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    scaled_delay(1000);
#endif
    more();
    you.one_time_ability_used.set(you.religion);
    take_note(Note(NOTE_GOD_GIFT, you.religion));
    return true;
}

bool fedhas_passthrough_class(const monster_type mc)
{
    return have_passive(passive_t::pass_through_plants)
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

// Basically we want to break a circle into n_arcs equal sized arcs and find
// out which arc the input point pos falls on.
static int _arc_decomposition(const coord_def & pos, int n_arcs)
{
    float theta = atan2((float)pos.y, (float)pos.x);

    if (pos.x == 0 && pos.y != 0)
        theta = pos.y > 0 ? PI / 2 : -PI / 2;

    if (theta < 0)
        theta += 2 * PI;

    float arc_angle = 2 * PI / n_arcs;

    theta += arc_angle / 2.0f;

    if (theta >= 2 * PI)
        theta -= 2 * PI;

    return static_cast<int> (theta / arc_angle);
}

int place_ring(vector<coord_def> &ring_points,
               const coord_def &origin,
               mgen_data prototype,
               int n_arcs,
               int arc_occupancy,
               int &seen_count)
{
    shuffle_array(ring_points);

    int target_amount = ring_points.size();
    int spawned_count = 0;
    seen_count = 0;

    vector<int> arc_counts(n_arcs, arc_occupancy);

    for (unsigned i = 0;
         spawned_count < target_amount && i < ring_points.size();
         i++)
    {
        int direction = _arc_decomposition(ring_points.at(i)
                                           - origin, n_arcs);

        if (arc_counts[direction]-- <= 0)
            continue;

        prototype.pos = ring_points.at(i);

        if (create_monster(prototype, false))
        {
            spawned_count++;
            if (you.see_cell(ring_points.at(i)))
                seen_count++;
        }
    }

    return spawned_count;
}

// Collect lists of points that are within LOS (under the given env map),
// unoccupied, and not solid (walls/statues).
void collect_radius_points(vector<vector<coord_def> > &radius_points,
                           const coord_def &origin, los_type los)
{
    radius_points.clear();
    radius_points.resize(LOS_RADIUS);

    // Just want to associate a point with a distance here for convenience.
    typedef pair<coord_def, int> coord_dist;

    // Using a priority queue because squares don't make very good circles at
    // larger radii. We will visit points in order of increasing euclidean
    // distance from the origin (not path distance). We want a min queue
    // based on the distance, so we use greater_second as the comparator.
    priority_queue<coord_dist, vector<coord_dist>,
                   greater_second<coord_dist> > fringe;

    fringe.push(coord_dist(origin, 0));

    set<int> visited_indices;

    int current_r = 1;
    int current_thresh = current_r * (current_r + 1);

    int max_distance = LOS_RADIUS * LOS_RADIUS + 1;

    while (!fringe.empty())
    {
        coord_dist current = fringe.top();
        // We're done here once we hit a point that is farther away from the
        // origin than our maximum permissible radius.
        if (current.second > max_distance)
            break;

        fringe.pop();

        int idx = current.first.x + current.first.y * X_WIDTH;
        if (!visited_indices.insert(idx).second)
            continue;

        while (current.second > current_thresh)
        {
            current_r++;
            current_thresh = current_r * (current_r + 1);
        }

        // We don't include radius 0. This is also a good place to check if
        // the squares are already occupied since we want to search past
        // occupied squares but don't want to consider them valid targets.
        if (current.second && !actor_at(current.first))
            radius_points[current_r - 1].push_back(current.first);

        for (adjacent_iterator i(current.first); i; ++i)
        {
            coord_dist temp(*i, current.second);

            // If the grid is out of LOS, skip it.
            if (!cell_see_cell(origin, temp.first, los))
                continue;

            coord_def local = temp.first - origin;

            temp.second = local.abs();

            idx = temp.first.x + temp.first.y * X_WIDTH;

            if (!visited_indices.count(idx)
                && in_bounds(temp.first)
                && !cell_is_solid(temp.first))
            {
                fringe.push(temp);
            }
        }

    }
}

static int _mushroom_prob(item_def & corpse)
{
    int low_threshold = 5;
    int high_threshold = FRESHEST_CORPSE - 5;

    // Expect this many trials over a corpse's lifetime since this function
    // is called once for every 10 units of rot_time.
    int step_size = 10;
    float total_trials = (high_threshold - low_threshold) / step_size;

    // Chance of producing no mushrooms (not really because of weight_factor
    // below).
    float p_failure = 0.5f;

    float trial_prob_f = 1 - powf(p_failure, 1.0f / total_trials);

    // The chance of producing mushrooms depends on the corpse capacity.
    // Take humans as the base factor here.
    float weight_factor = (float) max_corpse_chunks(corpse.mon_type) /
                          (float) max_corpse_chunks(MONS_HUMAN);

    trial_prob_f *= weight_factor;

    int trial_prob = static_cast<int>(100 * trial_prob_f);

    return trial_prob;
}

static bool _mushroom_spawn_message(int seen_targets, int seen_corpses)
{
    if (seen_targets <= 0)
        return false;

    string what  = seen_targets  > 1 ? "Some toadstools"
                                     : "A toadstool";
    string where = seen_corpses  > 1 ? "nearby corpses" :
                   seen_corpses == 1 ? "a nearby corpse"
                                     : "the ground";
    mprf("%s grow%s from %s.",
         what.c_str(), seen_targets > 1 ? "" : "s", where.c_str());

    return true;
}

// Place a partial ring of toadstools around the given corpse. Returns
// the number of mushrooms spawned. A return of 0 indicates no
// mushrooms were placed -> some sort of failure mode was reached.
static int _mushroom_ring(item_def &corpse, int & seen_count)
{
    // minimum number of mushrooms spawned on a given ring
    unsigned min_spawn = 2;

    seen_count = 0;

    vector<vector<coord_def> > radius_points;

    collect_radius_points(radius_points, corpse.pos, LOS_SOLID);

    // So what we have done so far is collect the set of points at each radius
    // reachable from the origin with (somewhat constrained) 8 connectivity,
    // now we will choose one of those radii and spawn mushrooms at some
    // of the points along it.
    int chosen_idx = random2(LOS_RADIUS);

    unsigned max_size = 0;
    for (unsigned i = 0; i < LOS_RADIUS; ++i)
    {
        if (radius_points[i].size() >= max_size)
        {
            max_size = radius_points[i].size();
            chosen_idx = i;
        }
    }

    chosen_idx = random2(chosen_idx + 1);

    // Not enough valid points?
    if (radius_points[chosen_idx].size() < min_spawn)
        return 0;

    mgen_data temp(MONS_TOADSTOOL, BEH_GOOD_NEUTRAL, coord_def(), MHITNOT,
                   MG_FORCE_PLACE);
    temp.set_col(corpse.get_colour());

    float target_arc_len = 2 * sqrtf(2.0f);

    int n_arcs = static_cast<int> (ceilf(2 * PI * (chosen_idx + 1)
                                   / target_arc_len));

    int spawned_count = place_ring(radius_points[chosen_idx], corpse.pos, temp,
                                   n_arcs, 1, seen_count);

    return spawned_count;
}

// Try to spawn 'target_count' mushrooms around the position of
// 'corpse'. Returns the number of mushrooms actually spawned.
// Mushrooms radiate outwards from the corpse following bfs with
// 8-connectivity. Could change the expansion pattern by using a
// priority queue for sequencing (priority = distance from origin under
// some metric).
static int _spawn_corpse_mushrooms(item_def& corpse,
                                  int target_count,
                                  int& seen_targets)

{
    seen_targets = 0;
    if (target_count == 0)
        return 0;

    int placed_targets = 0;

    queue<coord_def> fringe;
    set<int> visited_indices;

    // Slight chance of spawning a ring of mushrooms around the corpse (and
    // skeletonising it) if the corpse square is unoccupied.
    if (!actor_at(corpse.pos) && one_chance_in(100))
    {
        int ring_seen;
        // It's possible no reasonable ring can be found, in that case we'll
        // give up and just place a toadstool on top of the corpse (probably).
        int res = _mushroom_ring(corpse, ring_seen);

        if (res)
        {
            corpse.freshness = 0;

            if (you.see_cell(corpse.pos))
                mpr("A ring of toadstools grows before your very eyes.");
            else if (ring_seen > 1)
                mpr("Some toadstools grow in a peculiar arc.");
            else if (ring_seen > 0)
                mpr("A toadstool grows.");

            seen_targets = -1;

            return res;
        }
    }

    visited_indices.insert(X_WIDTH * corpse.pos.y + corpse.pos.x);
    fringe.push(corpse.pos);

    while (!fringe.empty())
    {
        coord_def current = fringe.front();

        fringe.pop();

        monster* mons = monster_at(current);

        bool player_occupant = you.pos() == current;

        // Is this square occupied by a non mushroom?
        if (mons && mons->mons_species() != MONS_TOADSTOOL
            || player_occupant && !you_worship(GOD_FEDHAS)
            || !can_spawn_mushrooms(current))
        {
            continue;
        }

        if (!mons)
        {
            monster *mushroom = create_monster(
                        mgen_data(MONS_TOADSTOOL,
                                  BEH_GOOD_NEUTRAL,
                                  current,
                                  MHITNOT,
                                  MG_FORCE_PLACE).set_col(corpse.get_colour()),
                                  false);

            if (mushroom)
            {
                // Going to explicitly override the die-off timer in
                // this case since, we're creating a lot of toadstools
                // at once that should die off quickly.
                coord_def offset = corpse.pos - current;

                int dist = static_cast<int>(sqrtf(offset.abs()) + 0.5);

                // Trying a longer base duration...
                int time_left = random2(8) + dist * 8 + 8;

                time_left *= 10;

                mon_enchant temp_en(ENCH_SLOWLY_DYING, 1, 0, time_left);
                mushroom->update_ench(temp_en);

                placed_targets++;
                if (current == you.pos())
                {
                    mprf("A toadstool grows %s.",
                         player_has_feet() ? "at your feet" : "before you");
                    current = mushroom->pos();
                }
                else if (you.see_cell(current))
                    seen_targets++;
            }
            else
                continue;
        }

        // We're done here if we placed the desired number of mushrooms.
        if (placed_targets == target_count)
            break;

        for (fair_adjacent_iterator ai(current); ai; ++ai)
        {
            if (in_bounds(*ai) && mons_class_can_pass(MONS_TOADSTOOL, grd(*ai)))
            {
                const int index = ai->x + ai->y * X_WIDTH;
                if (visited_indices.insert(index).second)
                    fringe.push(*ai); // Not previously visited.
            }
        }
    }

    return placed_targets;
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
                if (mons_skeleton(mons_zombie_base(*target)))
                {
                    simple_monster_message(*target, "'s flesh rots away.");

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
                simple_monster_message(*target, " rots away and dies.");

                kills = true;

                const coord_def pos = target->pos();
                const int colour = target->colour;
                const item_def* corpse = monster_die(target, KILL_MISC,
                                                     NON_MONSTER, true);

                // If a corpse didn't drop, create a toadstool.
                // If one did drop, we will create toadstools from it as usual
                // later on.
                // Give neither piety nor toadstools for summoned creatures.
                // Assumes that summoned creatures do not drop corpses (hence
                // will not give piety in the next loop).
                if (!corpse && piety)
                {
                    if (create_monster(
                                mgen_data(MONS_TOADSTOOL,
                                          BEH_GOOD_NEUTRAL,
                                          pos,
                                          MHITNOT,
                                          MG_FORCE_PLACE).set_col(colour)
                                       .set_summoned(&you, 0, 0),
                                          false))
                    {
                        seen_mushrooms++;
                    }

                    processed_count++;
                    continue;
                }

                // Verify that summoned creatures do not drop a corpse.
                ASSERT(!corpse || piety);

                break;
            }

            default:
                continue;
            }
        }

        for (stack_iterator j(*i); j; ++j)
        {
            bool corpse_on_pos = false;

            if (j->is_type(OBJ_CORPSES, CORPSE_BODY))
            {
                corpse_on_pos = true;

                const int trial_prob = _mushroom_prob(*j);
                const int target_count = 1 + binomial(20, trial_prob);
                int seen_per;
                _spawn_corpse_mushrooms(*j, target_count, seen_per);

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
        _mushroom_spawn_message(seen_mushrooms, seen_corpses);

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
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return processed_count;
}

static bool _create_plant(coord_def& target, int hp_adjust = 0)
{
    if (actor_at(target) || !mons_class_can_pass(MONS_PLANT, grd(target)))
        return 0;

    if (monster *plant = create_monster(mgen_data(MONS_PLANT,
                                            BEH_FRIENDLY,
                                            target,
                                            MHITNOT,
                                            MG_FORCE_PLACE,
                                            GOD_FEDHAS)
                                        .set_summoned(&you, 0, 0)))
    {
        plant->flags |= MF_NO_REWARD;
        plant->flags |= MF_ATT_CHANGE_ATTEMPT;

        mons_make_god_gift(*plant, GOD_FEDHAS);

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
        const int expiry = you.elapsed_time + SUNLIGHT_DURATION;
        env.sunlight.emplace_back(*ai, expiry);

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
    for (coord_def c : origins)
    {
        int idx = c.x + c.y * X_WIDTH;
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
            && you.can_see(*hostile))
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
            available_fruit.emplace_back(you.inv[i].quantity, i);
        }
    }
    sort(available_fruit.begin(), available_fruit.end());

    return total;
}

static void _decrease_amount(vector<pair<int, int> >& available, int amount)
{
    const int total_decrease = amount;
    for (const auto &avail : available)
    {
        const int decrease_amount = min(avail.first, amount);
        amount -= decrease_amount;
        dec_inv_item_quantity(avail.second, decrease_amount);
    }
    if (total_decrease > 1)
        mprf("%d pieces of fruit are consumed!", total_decrease);
    else
        mpr("A piece of fruit is consumed!");
}

// Create a ring or partial ring around the caster. The user is
// prompted to select a stack of fruit, and then plants are placed on open
// squares adjacent to the user. Of course, one piece of fruit is
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
// approximately 2. This turns normal floor tiles into shallow water
// and turns (unoccupied) shallow water into deep water. There is a
// chance of spawning plants or fungus on unoccupied dry floor tiles
// outside of the rainfall area. Return the number of plants/fungi
// created.
int fedhas_rain(const coord_def &target)
{
    int spawned_count = 0;
    int processed_count = 0;

    for (radius_iterator rad(target, LOS_NO_TRANS, true); rad; ++rad)
    {
        int rain_thresh = 2;
        coord_def local = *rad - target;

        dungeon_feature_type ftype = grd(*rad);

        if (local.rdist() > rain_thresh)
        {
            // Maybe spawn a plant on (dry, open) squares that are in
            // LOS but outside the rainfall area. In open space, there
            // are 225 squares in LOS, and we are going to drop water on
            // 25 of those, so if we want x plants to spawn on
            // average in open space, the trial probability should be
            // x/200.
            if (x_chance_in_y(5, 200)
                && !actor_at(*rad)
                && ftype == DNGN_FLOOR)
            {
                if (create_monster(mgen_data(
                                      coinflip() ? MONS_PLANT : MONS_FUNGUS,
                                      BEH_GOOD_NEUTRAL,
                                      *rad,
                                      MHITNOT,
                                      MG_FORCE_PLACE,
                                      GOD_FEDHAS)
                                   .set_summoned(&you, 0, 0)))
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
        // shallow water. Destroying items will probably be annoying,
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
            // The rainfall area is 24 (5*5 - 1 (center));
            // the expected number of clouds generated by a fixed chance
            // per tile is 24 * p = expected. Say an Invocations skill
            // of 27 gives expected 6 clouds.
            int max_expected = 6;
            int expected = you.skill_rdiv(SK_INVOCATIONS, max_expected, 27);

            if (x_chance_in_y(expected, 24))
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
            if (stack_it->is_type(OBJ_CORPSES, CORPSE_BODY))
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
    for (const stack_iterator &si : positions)
    {
#ifndef USE_TILE_LOCAL
        coord_def temp = grid2view(si->pos);
        cgotoxy(temp.x, temp.y, GOTO_DNGN);

        unsigned colour = GREEN | COLFLAG_FRIENDLY_MONSTER;
        colour = real_colour(colour);

        unsigned character = mons_char(MONS_BALLISTOMYCETE_SPORE);
        put_colour_ch(colour, character);
#endif
#ifdef USE_TILE
        tiles.add_overlay(si->pos, TILE_SPORE_OVERLAY);
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
// and make 1 ballistomycete spore per corpse. Spores are given the input as
// their starting behavior; the function returns the number of corpses
// processed.
int fedhas_corpse_spores(beh_type attitude)
{
    vector<stack_iterator> positions;
    int count = count_corpses_in_los(&positions);
    ASSERT(attitude != BEH_FRIENDLY || count > 0);

    if (count == 0)
        return count;

    for (const stack_iterator &si : positions)
    {
        count++;

        if (monster *plant = create_monster(mgen_data(MONS_BALLISTOMYCETE_SPORE,
                                               attitude,
                                               si->pos,
                                               MHITNOT,
                                               MG_FORCE_PLACE,
                                               GOD_FEDHAS)
                                            .set_summoned(&you, 0, 0)))
        {
            plant->flags |= MF_NO_REWARD;

            if (attitude == BEH_FRIENDLY)
            {
                plant->flags |= MF_ATT_CHANGE_ATTEMPT;

                mons_make_god_gift(*plant, GOD_FEDHAS);

                plant->behaviour = BEH_WANDER;
                plant->foe = MHITNOT;
            }
        }

        if (mons_skeleton(si->mon_type))
            turn_corpse_into_skeleton(*si);
        else
        {
            item_was_destroyed(*si);
            destroy_item(si->index());
        }
    }

    viewwindow(false);

    return count;
}

struct monster_conversion
{
    monster_type new_type;
    int piety_cost;
    int fruit_cost;
};

static const map<monster_type, monster_conversion> conversions =
{
    { MONS_PLANT,          { MONS_OKLOB_PLANT, 0, 1 } },
    { MONS_BUSH,           { MONS_OKLOB_PLANT, 0, 1 } },
    { MONS_BURNING_BUSH,   { MONS_OKLOB_PLANT, 0, 1 } },
    { MONS_OKLOB_SAPLING,  { MONS_OKLOB_PLANT, 4, 0 } },
    { MONS_FUNGUS,         { MONS_WANDERING_MUSHROOM, 3, 0 } },
    { MONS_TOADSTOOL,      { MONS_WANDERING_MUSHROOM, 3, 0 } },
    { MONS_BALLISTOMYCETE, { MONS_HYPERACTIVE_BALLISTOMYCETE, 4, 0 } },
};

bool mons_is_evolvable(const monster* mon)
{
    return conversions.count(mon->type) && !mon->has_ench(ENCH_PETRIFIED);
}

bool fedhas_check_evolve_flora(bool quiet)
{
    for (monster_near_iterator mi(&you, LOS_NO_TRANS); mi; ++mi)
        if (mons_is_evolvable(*mi))
            return true;

    if (!quiet)
        mpr("No evolvable flora in sight.");
    return false;
}

static vector<string> _evolution_name(const monster_info& mon)
{
    auto conv = map_find(conversions, mon.type);
    if (conv && !mon.has_trivial_ench(ENCH_PETRIFIED))
        return { "can evolve into " + mons_type_name(conv->new_type, DESC_A) };
    else
        return { "cannot be evolved" };
}

spret_type fedhas_evolve_flora(bool fail)
{
    dist spelld;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_EVOLVABLE_PLANTS;
    args.range = LOS_RADIUS;
    args.needs_path = false;
    args.self = CONFIRM_CANCEL;
    args.show_floor_desc = true;
    args.top_prompt = "Select plant or fungus to evolve.";
    args.get_desc_func = _evolution_name;

    direction(spelld, args);

    if (!spelld.isValid)
    {
        // Check for user cancel.
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    monster* const plant = monster_at(spelld.target);

    if (!plant)
    {
        if (feat_is_tree(env.grid(spelld.target)))
            mpr("The tree has already reached the pinnacle of evolution.");
        else
            mpr("You must target a plant or fungus.");
        return SPRET_ABORT;
    }

    if (!mons_is_evolvable(plant))
    {
        if (plant->type == MONS_BALLISTOMYCETE_SPORE)
            mpr("You can evolve only complete plants, not seeds.");
        else if (!mons_is_plant(*plant))
            mpr("Only plants or fungi may be evolved.");
        else if (plant->has_ench(ENCH_PETRIFIED))
            mpr("Stone cannot grow or evolve.");
        else
        {
            simple_monster_message(*plant, " has already reached the pinnacle"
                                   " of evolution.");
        }

        return SPRET_ABORT;
    }

    monster_conversion upgrade = *map_find(conversions, plant->type);

    vector<pair<int, int> > collected_fruit;
    if (upgrade.fruit_cost)
    {
        const int total_fruit = _collect_fruit(collected_fruit);

        if (total_fruit < upgrade.fruit_cost)
        {
            mpr("Not enough fruit available.");
            return SPRET_ABORT;
        }
    }

    if (upgrade.piety_cost && upgrade.piety_cost > you.piety)
    {
        mpr("Not enough piety available.");
        return SPRET_ABORT;
    }

    fail_check();

    switch (upgrade.new_type)
    {
    case MONS_OKLOB_PLANT:
    {
        if (plant->type == MONS_OKLOB_SAPLING)
            simple_monster_message(*plant, " appears stronger.");
        else
        {
            string evolve_desc = " can now spit acid";
            const int skill = you.skill(SK_INVOCATIONS);
            if (skill >= 20)
                evolve_desc += " continuously";
            else if (skill >= 15)
                evolve_desc += " quickly";
            else if (skill >= 10)
                evolve_desc += " rather quickly";
            else if (skill >= 5)
                evolve_desc += " somewhat quickly";
            evolve_desc += ".";

            simple_monster_message(*plant, evolve_desc.c_str());
        }
        break;
    }

    case MONS_WANDERING_MUSHROOM:
        simple_monster_message(*plant, " can now pick up its mycelia and move.");
        break;

    case MONS_HYPERACTIVE_BALLISTOMYCETE:
        simple_monster_message(*plant, " appears agitated.");
        env.level_state |= LSTATE_GLOW_MOLD;
        break;

    default:
        break;
    }

    plant->upgrade_type(upgrade.new_type, true, true);

    plant->flags |= MF_NO_REWARD;
    plant->flags |= MF_ATT_CHANGE_ATTEMPT;

    mons_make_god_gift(*plant, GOD_FEDHAS);

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
        // frequency will be set by set_hit_dice below
        plant->spells = { { SPELL_SPIT_ACID, 0, MON_SPELL_NATURAL } };
    }

    plant->set_hit_dice(plant->get_experience_level()
                        + you.skill_rdiv(SK_INVOCATIONS));

    if (upgrade.fruit_cost)
        _decrease_amount(collected_fruit, upgrade.fruit_cost);

    if (upgrade.piety_cost)
    {
        lose_piety(upgrade.piety_cost);
        mpr("Your piety has decreased.");
    }

    return SPRET_SUCCESS;
}

static bool _lugonu_warp_monster(monster& mon, int pow)
{
    if (coinflip())
        return false;

    if (!mon.friendly())
        behaviour_event(&mon, ME_ANNOY, &you);

    mon.hurt(&you, 1 + random2(pow / 6));

    if (mon.alive() && !mon.no_tele(true, false))
        mon.blink();

    return true;
}

static void _lugonu_warp_area(int pow)
{
    apply_monsters_around_square([pow] (monster& mon) {
        return _lugonu_warp_monster(mon, pow);
    }, you.pos());
}

void lugonu_bend_space()
{
    const int pow = 4 + skill_bump(SK_INVOCATIONS);
    const bool area_warp = random2(pow) > 9;

    mprf("Space bends %saround you!", area_warp ? "sharply " : "");

    if (area_warp)
        _lugonu_warp_area(pow);

    uncontrolled_blink(true);

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
                     mon->resist_margin_phrase(res_margin).c_str());
                continue;
            }

            simple_god_message(
                make_stringf(" rebukes %s.",
                             mon->name(DESC_THE).c_str()).c_str(),
                             GOD_CHEIBRIADOS);
            do_slow_monster(*mon, &you);
        }
    }
}

static int _slouch_damage(monster *mon)
{
    // Please change handle_monster_move in mon-act.cc to match.
    const int jerk_num = mon->type == MONS_SIXFIRHY ? 8
                       : mon->type == MONS_JIANGSHI ? 48
                                                    : 1;

    const int jerk_denom = mon->type == MONS_SIXFIRHY ? 24
                         : mon->type == MONS_JIANGSHI ? 90
                                                      : 1;

    const int player_numer = BASELINE_DELAY * BASELINE_DELAY * BASELINE_DELAY;
    return 4 * (mon->speed * BASELINE_DELAY * jerk_num
                           / mon->action_energy(EUT_MOVE) / jerk_denom
                - player_numer / player_movement_speed() / player_speed());
}

static bool _slouchable(coord_def where)
{
    monster* mon = monster_at(where);
    if (mon == nullptr || mon->is_stationary() || mon->cannot_move()
        || mons_is_projectile(mon->type)
        || mon->asleep() && !mons_is_confused(*mon))
    {
        return false;
    }

    return _slouch_damage(mon) > 0;
}

static bool _act_slouchable(const actor *act)
{
    if (act->is_player())
        return false;  // too slow-witted
    return _slouchable(act->pos());
}

static int _slouch_monsters(coord_def where)
{
    if (!_slouchable(where))
        return 0;

    monster* mon = monster_at(where);
    ASSERT(mon);

    // Between 1/2 and 3/2 of _slouch_damage(), but weighted strongly
    // towards the middle.
    const int dmg = roll_dice(_slouch_damage(mon), 3) / 2;

    mon->hurt(&you, dmg, BEAM_MMISSILE, KILLED_BY_BEAM, "", "", true);
    return 1;
}

bool cheibriados_slouch()
{
    int count = apply_area_visible(_slouchable, you.pos());
    if (!count)
        if (!yesno("There's no one hasty visible. Invoke Slouch anyway?",
                   true, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }

    targetter_los hitfunc(&you, LOS_DEFAULT);
    if (stop_attack_prompt(hitfunc, "harm", _act_slouchable))
        return false;

    mpr("You can feel time thicken for a moment.");
    dprf("your speed is %d", player_movement_speed());

    apply_area_visible(_slouch_monsters, you.pos());
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

    if (monster *mon = monster_at(old_pos))
    {
        mon->props[FAKE_BLINK_KEY].get_bool() = true;
        mon->blink();
        mon->props.erase(FAKE_BLINK_KEY);
        if (monster *stubborn = monster_at(old_pos))
            monster_teleport(stubborn, true, true);
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
    // Update corpses, etc. This does also shift monsters, but only by
    // a tiny bit.
    update_level(pow * 10);

#ifndef USE_TILE_LOCAL
    scaled_delay(1000);
#endif

    if (monster *mon = monster_at(old_pos))
    {
        mon->props[FAKE_BLINK_KEY].get_bool() = true;
        mon->blink();
        mon->props.erase(FAKE_BLINK_KEY);
        if (monster *stubborn = monster_at(old_pos))
            monster_teleport(stubborn, true, true);
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
            return false;
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

/**
 * Give a prompt to curse an item.
 *
 * This is the core logic behind Ash's Curse Item ability.
 * Player can abort without penalty.
 * Player can curse any cursable item (not just worn ones).
 *
 * @param num_rc Number of remove curse scrolls available.
 * @return       Whether the player cursed anything.
 */
bool ashenzari_curse_item(int num_rc)
{
    ASSERT(num_rc > 0);
    const string prompt_msg = make_stringf(
            "Curse which item? (%d remove curse scroll%s left)"
            " (Esc to abort)",
            num_rc, num_rc == 1 ? "" : "s");
    const int item_slot = prompt_invent_item(prompt_msg.c_str(), MT_INVLIST,
                                             OSEL_CURSABLE,
                                             true, true, false);
    if (prompt_failed(item_slot))
        return false;

    item_def& item(you.inv[item_slot]);

    if (!item_is_cursable(item))
    {
        mpr("You can't curse that!");
        return false;
    }

    do_curse_item(item, false);
    learned_something_new(HINT_YOU_CURSED);
    return true;
}

bool can_convert_to_beogh()
{
    if (silenced(you.pos()))
        return false;

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        if (mons_allows_beogh_now(**mi))
            return true;

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
        if (mons_allows_beogh(*mon))
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
        mons_pacify(*orc, ATT_GOOD_NEUTRAL, true);
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
    ASSERT(you.umbra_radius() > -1);
    const int range = you.umbra_radius();

    targetter_shadow_step tgt(&you, you.umbra_radius());
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
            trap_def* trap = trap_at(site);
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

int gozag_potion_price()
{
    if (!you.attribute[ATTR_GOZAG_FIRST_POTION])
        return 0;

    return GOZAG_POTION_PETITION_AMOUNT;
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
                int multiplier = random_range(20, 30); // arbitrary

                if (!you.attribute[ATTR_GOZAG_FIRST_POTION])
                    multiplier = 0;

                string key = make_stringf(GOZAG_POTIONS_KEY, i);
                you.props.erase(key);
                you.props[key].new_vector(SV_INT, SFLAG_CONST_TYPE);
                pots[i] = &you.props[key].get_vector();

                ADD_POTIONS(*pots[i], _gozag_potion_list);
                if (coinflip())
                    ADD_POTIONS(*pots[i], _gozag_potion_list);

                for (const CrawlStoreValue& store : *pots[i])
                {
                    dummy.sub_type = store.get_int();
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

    while (true)
    {
        if (crawl_state.seen_hups)
            return false;

        clear_messages();
        for (int i = 0; i < GOZAG_MAX_POTIONS; i++)
        {
            string line = make_stringf("  [%c] - %d gold - ", i + 'a',
                                       prices[i]);
            vector<string> pot_names;
            for (const CrawlStoreValue& store : *pots[i])
                pot_names.emplace_back(potion_type_name(store.get_int()));
            line += comma_separated_line(pot_names.begin(), pot_names.end());
            mpr_nojoin(MSGCH_PLAIN, line);
        }
        mprf(MSGCH_PROMPT, "Purchase which effect?");
        keyin = toalower(get_ch()) - 'a';
        if (keyin < 0 || keyin > GOZAG_MAX_POTIONS - 1)
            continue;

        if (you.gold < prices[keyin])
        {
            mpr("You don't have enough gold for that!");
            more();
            continue;
        }

        break;
    }

    ASSERT(you.gold >= prices[keyin]);
    you.del_gold(prices[keyin]);
    you.attribute[ATTR_GOZAG_GOLD_USED] += prices[keyin];

    for (auto pot : *pots[keyin])
        potionlike_effect(static_cast<potion_type>(pot.get_int()), 40);

    if (!you.attribute[ATTR_GOZAG_FIRST_POTION])
        you.attribute[ATTR_GOZAG_FIRST_POTION] = 1;

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
    const int max_base = 800;
    const int base = max ? max_base : random_range(max_base/2, max_base);
    const int price = base
                      * (GOZAG_SHOP_BASE_MULTIPLIER
                         + GOZAG_SHOP_MOD_MULTIPLIER
                           * you.attribute[ATTR_GOZAG_SHOPS])
                      / GOZAG_SHOP_BASE_MULTIPLIER;
    return price;
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
    if (!is_connected_branch(level_id::current().branch))
    {
        if (!quiet)
        {
            mprf("No merchants are willing to come to this location.");
            return false;
        }
    }
    if (grd(you.pos()) != DNGN_FLOOR)
    {
        if (!quiet)
        {
            mprf("You need to be standing on open floor to call a merchant.");
            return false;
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
 * index?
 */
static int _gozag_shop_price(int index)
{
    ASSERT(_gozag_valid_shop_index(index));

    return you.props[make_stringf(GOZAG_SHOP_COST_KEY, index)].get_int();
}

/**
 * Initialize the set of shops currently offered to the player through Call
 * Merchant.
 *
 * @param index       The index of the shop offer to be defined.
 * @param valid_shops Vector of acceptable shop types based on the player and
 *                    previous choices for this merchant call.
*/
static void _setup_gozag_shop(int index, vector<shop_type> &valid_shops)
{
    ASSERT(!you.props.exists(make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, index)));

    shop_type type = NUM_SHOPS;
    if (index == 0 && !you_foodless_normally())
        type = SHOP_FOOD;
    else
    {
        int choice = random2(valid_shops.size());
        type = valid_shops[choice];
        // Don't choose this shop type again for this merchant call.
        valid_shops.erase(valid_shops.begin() + choice);
    }
    you.props[make_stringf(GOZAG_SHOP_TYPE_KEY, index)].get_int() = type;

    you.props[make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, index)].get_string()
                                    = make_name();

    const bool need_suffix = type != SHOP_GENERAL
                             && type != SHOP_GENERAL_ANTIQUE
                             && type != SHOP_DISTILLERY;
    you.props[make_stringf(GOZAG_SHOP_SUFFIX_KEY, index)].get_string()
                                    = need_suffix
                                      ? random_choose("Shoppe", "Boutique",
                                                      "Emporium", "Shop")
                                      : "";

    you.props[make_stringf(GOZAG_SHOP_COST_KEY, index)].get_int()
        = gozag_price_for_shop();
}

/**
 * If Gozag's version of a given shop type has a special name, what is it?
 *
 * @param type      The type of shop in question.
 * @return          A special name for the shop (replacing its type-name) if
 *                  appropriate, or an empty string otherwise.
 */
static string _gozag_special_shop_name(shop_type type)
{
    if (type == SHOP_FOOD)
    {
        if (you.species == SP_VAMPIRE)
            return "Blood";
        else if (you.species == SP_GHOUL)
            return "Carrion"; // yum!
    }

    return "";
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
    const string special_name = _gozag_special_shop_name(type);
    const string type_name = !special_name.empty() ?
                                special_name :
                                shop_type_name(type);
    const string suffix =
        you.props[make_stringf(GOZAG_SHOP_SUFFIX_KEY, index)].get_string();

    return make_stringf("  [%c] %5d gold - %s %s %s",
                        offer_letter,
                        cost,
                        shop_name.c_str(),
                        type_name.c_str(),
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

    string spec_type = _gozag_special_shop_name(type);
    if (!spec_type.empty())
        spec_type = " type:" + spec_type;

    return make_stringf("%s shop name:%s%s%s gozag",
                        shoptype_to_str(type),
                        replace_all(name, " ", "_").c_str(),
                        suffix.c_str(),
                        spec_type.c_str());

}

/**
 * Attempt to call the shop specified at the given index at your position.
 *
 * @param index     The index of the shop (in gozag props)
 */
static void _gozag_place_shop(int index)
{
    ASSERT(grd(you.pos()) == DNGN_FLOOR);
    keyed_mapspec kmspec;
    kmspec.set_feat(_gozag_shop_spec(index), false);
    if (!kmspec.get_feat().shop.get())
        die("Invalid shop spec?");

    feature_spec feat = kmspec.get_feat();
    shop_spec *spec = feat.shop.get();
    ASSERT(spec);
    place_spec_shop(you.pos(), *spec, you.experience_level);

    link_items();
    env.markers.add(new map_feature_marker(you.pos(), DNGN_ABANDONED_SHOP));
    env.markers.clear_need_activate();

    shop_struct *shop = shop_at(you.pos());
    ASSERT(shop);

    const gender_type gender = random_choose(GENDER_FEMALE, GENDER_MALE,
                                             GENDER_NEUTER);

    mprf(MSGCH_GOD, "%s invites you to visit %s %s%s%s.",
                    shop->shop_name.c_str(),
                    decline_pronoun(gender, PRONOUN_POSSESSIVE),
                    shop_type_name(shop->type).c_str(),
                    !shop->shop_suffix_name.empty() ? " " : "",
                    shop->shop_suffix_name.c_str());
}

bool gozag_call_merchant()
{
    // Only offer useful shops.
    vector<shop_type> valid_shops;
    for (int i = 0; i < NUM_SHOPS; i++)
    {
        shop_type type = static_cast<shop_type>(i);
        // if they are useful to the player, food shops are handled through the
        // first index.
        if (type == SHOP_FOOD)
            continue;
        if (type == SHOP_DISTILLERY && you.species == SP_MUMMY)
            continue;
        if (type == SHOP_EVOKABLES && player_mutation_level(MUT_NO_ARTIFICE))
            continue;
        if (you.species == SP_FELID &&
            (type == SHOP_ARMOUR
             || type == SHOP_ARMOUR_ANTIQUE
             || type == SHOP_WEAPON
             || type == SHOP_WEAPON_ANTIQUE))
        {
            continue;
        }
        valid_shops.push_back(type);
    }

    // Set up some dummy shops.
    // Generate some shop inventory and store it as a store spec.
    // We still set up the shops in advance in case of hups.
    for (int i = 0; i < _gozag_max_shops(); i++)
        if (!you.props.exists(make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, i)))
            _setup_gozag_shop(i, valid_shops);

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

branch_type gozag_fixup_branch(branch_type branch)
{
    if (is_hell_subbranch(branch))
        return BRANCH_VESTIBULE;

    return branch;
}

static const map<branch_type, int> branch_bribability_factor =
{
    { BRANCH_DUNGEON,     2 },
    { BRANCH_ORC,         2 },
    { BRANCH_ELF,         3 },
    { BRANCH_SNAKE,       3 },
    { BRANCH_SHOALS,      3 },
    { BRANCH_CRYPT,       3 },
    { BRANCH_TOMB,        3 },
    { BRANCH_DEPTHS,      4 },
    { BRANCH_VAULTS,      4 },
    { BRANCH_ZOT,         4 },
    { BRANCH_VESTIBULE,   4 },
    { BRANCH_PANDEMONIUM, 4 },
};

// An x-in-8 chance of a monster of the given type being bribed.
// Tougher monsters have a stronger chance of being bribed.
int gozag_type_bribable(monster_type type)
{
    if (!you_worship(GOD_GOZAG))
        return 0;

    if (mons_class_intel(type) < I_HUMAN)
        return 0;

    // Unique rune guardians can't be bribed, sorry!
    if (mons_is_unique(type)
        && (mons_genus(type) == MONS_HELL_LORD
            || mons_genus(type) == MONS_PANDEMONIUM_LORD
            || type == MONS_ANTAEUS))
    {
        return 0;
    }

    const int *factor = map_find(branch_bribability_factor,
                                 gozag_fixup_branch(you.where_are_you));
    if (!factor)
        return 0;

    const int chance = max(mons_class_hit_dice(type) / *factor, 1);
    dprf("%s, bribe chance: %d", mons_type_name(type, DESC_PLAIN).c_str(),
                                 chance);

    return chance;
}

bool gozag_branch_bribable(branch_type branch)
{
    return map_find(branch_bribability_factor, gozag_fixup_branch(branch));
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
    branch_type branch = gozag_fixup_branch(you.where_are_you);
    if (feat_is_branch_entrance(grd(you.pos())))
    {
        for (branch_iterator it; it; ++it)
            if (it->entry_stairs == grd(you.pos())
                && gozag_branch_bribable(it->id))
            {
                branch_type stair_branch = gozag_fixup_branch(it->id);
                string prompt =
                    make_stringf("Do you want to bribe the denizens of %s?",
                                 stair_branch == BRANCH_VESTIBULE ? "the Hells"
                                 : branches[stair_branch].longname);
                if (yesno(prompt.c_str(), true, 'n'))
                {
                    branch = stair_branch;
                    prompted = true;
                }
                // If we're in the Vestibule, standing on a portal to a Hell
                // sub-branch, don't prompt twice to bribe the Hells.
                else if (branch == stair_branch)
                {
                    canned_msg(MSG_OK);
                    return false;
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
                     branch == BRANCH_VESTIBULE ? "the Hells" :
                     branches[branch].longname);

    if (prompted || yesno(prompt.c_str(), true, 'n'))
    {
        you.del_gold(bribe_amount);
        you.attribute[ATTR_GOZAG_GOLD_USED] += bribe_amount;
        branch_bribe[branch] += bribe_amount;
        string msg = make_stringf(" spreads your bribe to %s!",
                                  branch == BRANCH_VESTIBULE ? "the Hells" :
                                  branches[branch].longname);
        simple_god_message(msg.c_str());
        add_daction(DACT_SET_BRIBES);
        return true;
    }

    canned_msg(MSG_OK);
    return false;
}

static int _upheaval_radius(int pow)
{
    return pow >= 100 ? 2 : 1;
}

spret_type qazlal_upheaval(coord_def target, bool quiet, bool fail)
{
    int pow = you.skill(SK_INVOCATIONS, 6);
    const int max_radius = _upheaval_radius(pow);

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
        direction_chooser_args args;
        args.restricts = DIR_TARGET;
        args.mode = TARG_HOSTILE;
        args.needs_path = false;
        args.top_prompt = "Aiming: <white>Upheaval</white>";
        args.self = CONFIRM_CANCEL;
        args.hitfunc = &tgt;
        if (!spell_direction(spd, beam, &args))
            return SPRET_ABORT;

        if (cell_is_solid(beam.target))
        {
            mprf("There is %s there.",
                 article_a(feat_type_name(grd(beam.target))).c_str());
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
    for (radius_iterator ri(beam.target, max_radius, C_SQUARE, LOS_SOLID, true);
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

    for (coord_def pos : affected)
    {
        beam.draw(pos);
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

    for (coord_def pos : affected)
    {
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
                if (!cell_is_solid(pos) && !cloud_at(pos) && coinflip())
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

spret_type qazlal_elemental_force(bool fail)
{
    static const map<cloud_type, monster_type> elemental_clouds = {
        { CLOUD_FIRE,           MONS_FIRE_ELEMENTAL },
        { CLOUD_FOREST_FIRE,    MONS_FIRE_ELEMENTAL },
        { CLOUD_COLD,           MONS_WATER_ELEMENTAL },
        { CLOUD_RAIN,           MONS_WATER_ELEMENTAL },
        { CLOUD_DUST,           MONS_EARTH_ELEMENTAL },
        { CLOUD_PETRIFY,        MONS_EARTH_ELEMENTAL },
        { CLOUD_BLACK_SMOKE,    MONS_AIR_ELEMENTAL },
        { CLOUD_GREY_SMOKE,     MONS_AIR_ELEMENTAL },
        { CLOUD_BLUE_SMOKE,     MONS_AIR_ELEMENTAL },
        { CLOUD_PURPLE_SMOKE,   MONS_AIR_ELEMENTAL },
        { CLOUD_STORM,          MONS_AIR_ELEMENTAL },
    };

    vector<coord_def> targets;
    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_SQUARE, true); ri; ++ri)
    {
        const cloud_struct* cloud = cloud_at(*ri);
        if (!cloud || !elemental_clouds.count(cloud->type))
            continue;

        const actor *agent = actor_by_mid(cloud->source);
        if (agent && agent->is_player())
            targets.push_back(*ri);
    }

    if (targets.empty())
    {
        mpr("You can't see any clouds you can empower.");
        return SPRET_ABORT;
    }

    fail_check();

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
        ASSERT(cloud_at(pos));
        const cloud_struct &cl = *cloud_at(pos);
        mg.behaviour = BEH_FRIENDLY;
        mg.pos       = pos;
        mg.cls = *map_find(elemental_clouds, cl.type);
        if (!create_monster(mg))
            continue;
        delete_cloud(pos);
        placed++;
    }

    if (placed)
        mprf(MSGCH_GOD, "Clouds arounds you coalesce and take form!");
    else
        canned_msg(MSG_NOTHING_HAPPENS); // can this ever happen?

    return SPRET_SUCCESS;
}

bool qazlal_disaster_area()
{
    bool friendlies = false;
    vector<coord_def> targets;
    vector<int> weights;
    const int pow = you.skill(SK_INVOCATIONS, 6);
    const int upheaval_radius = _upheaval_radius(pow);
    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_SQUARE, LOS_NO_TRANS, true);
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

        const int range = you.pos().distance_from(*ri);
        const int dist = grid_distance(you.pos(), *ri);
        if (range <= upheaval_radius)
            continue;

        targets.push_back(*ri);
        // We weight using the square of grid distance, so monsters fewer tiles
        // away are more likely to be hit.
        int weight = LOS_RADIUS * LOS_RADIUS + 1 - dist * dist;
        if (actor_at(*ri))
            weight *= 10;
        weights.push_back(weight);
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

/// A map between sacrifice_def.sacrifice_vector strings & possible mut lists
static map<const char*, vector<mutation_type>> sacrifice_vector_map =
{
    /// Mutations granted by ABIL_RU_SACRIFICE_HEALTH
    { HEALTH_SAC_KEY, {
        MUT_FRAIL,
        MUT_PHYSICAL_VULNERABILITY,
        MUT_SLOW_REFLEXES,
    }},
    /// Mutations granted by ABIL_RU_SACRIFICE_ESSENCE
    { ESSENCE_SAC_KEY, {
        MUT_ANTI_WIZARDRY,
        MUT_MAGICAL_VULNERABILITY,
        MUT_MAGICAL_VULNERABILITY,
        MUT_LOW_MAGIC,
    }},
    /// Mutations granted by ABIL_RU_SACRIFICE_PURITY
    { PURITY_SAC_KEY, {
        MUT_SCREAM,
        MUT_SLOW_REGENERATION,
        MUT_NO_DEVICE_HEAL,
        MUT_DOPEY,
        MUT_CLUMSY,
        MUT_WEAK,
    }},
};

/// School-disabling mutations that will be painful for most characters.
static const vector<mutation_type> _major_arcane_sacrifices =
{
    MUT_NO_CHARM_MAGIC,
    MUT_NO_CONJURATION_MAGIC,
    MUT_NO_SUMMONING_MAGIC,
    MUT_NO_TRANSLOCATION_MAGIC,
};

/// School-disabling mutations that are unfortunate for most characters.
static const vector<mutation_type> _moderate_arcane_sacrifices =
{
    MUT_NO_TRANSMUTATION_MAGIC,
    MUT_NO_NECROMANCY_MAGIC,
    MUT_NO_HEXES_MAGIC,
};

/// School-disabling mutations that are mostly easy to deal with.
static const vector<mutation_type> _minor_arcane_sacrifices =
{
    MUT_NO_AIR_MAGIC,
    MUT_NO_FIRE_MAGIC,
    MUT_NO_ICE_MAGIC,
    MUT_NO_EARTH_MAGIC,
    MUT_NO_POISON_MAGIC,
};

/// The list of all lists of arcana sacrifice mutations.
static const vector<mutation_type> _arcane_sacrifice_lists[] =
{
    _minor_arcane_sacrifices,
    _moderate_arcane_sacrifices,
    _major_arcane_sacrifices,
};

/**
 * Choose a random mutation from the given list, only including those that are
 * valid choices for a Ru sacrifice. (Not already at the max level, not
 * conflicting with an innate mut.)
 *
 * @param muts      The list of possible sacrifice mutations.
 * @return          A mutation from the list, or MUT_NON_MUTATION if no valid
 *                  result was found.
 */
static mutation_type _random_valid_sacrifice(const vector<mutation_type> &muts)
{
    int valid_sacrifices = 0;
    mutation_type chosen_sacrifice = MUT_NON_MUTATION;
    for (auto mut : muts)
    {
        // can't give the player this if they're already at max
        if (player_mutation_level(mut) >= mutation_max_levels(mut))
            continue;

        // can't give the player this if they have an innate mut that conflicts
        if (mut_check_conflict(mut, true))
            continue;

        // special case a few weird interactions
        // vampires can't get slow regeneration for some reason related to
        // their existing regen silliness
        if (you.species == SP_VAMPIRE && mut == MUT_SLOW_REGENERATION)
            continue;

        // demonspawn can't get frail if they have a robust facet
        if (you.species == SP_DEMONSPAWN && mut == MUT_FRAIL
            && any_of(begin(you.demonic_traits), end(you.demonic_traits),
                      [] (player::demon_trait t)
                      { return t.mutation == MUT_ROBUST; }))
        {
            continue;
        }

        // The Grunt Algorithm
        // (choose a random element from a set of unknown size without building
        // an explicit list, by giving each one a chance to be chosen equal to
        // the size of the known list so far, but not returning until the whole
        // set has been seen.)
        // TODO: export this to a function?
        ++valid_sacrifices;
        if (one_chance_in(valid_sacrifices))
            chosen_sacrifice = mut;
    }

    return chosen_sacrifice;
}

/**
 * Choose a random valid mutation from the given list & insert it into the
 * single-element vector player prop.
 *
 * @param key           The key of the player prop to insert the mut into.
 */
static void _choose_sacrifice_mutation(const char *key)
{
    ASSERT(you.props.exists(key));
    CrawlVector &current_sacrifice = you.props[key].get_vector();
    ASSERT(current_sacrifice.empty());

    const mutation_type mut
        = _random_valid_sacrifice(sacrifice_vector_map[key]);
    if (mut != MUT_NON_MUTATION)
    {
        // XXX: why on earth is this a one-element vector?
        current_sacrifice.push_back(static_cast<int>(mut));
    }
}

/**
 * Choose a set of three spellschools to sacrifice: one major, one moderate,
 * and one minor.
 */
static void _choose_arcana_mutations()
{
    ASSERT(you.props.exists(ARCANA_SAC_KEY));
    CrawlVector &current_arcane_sacrifices
        = you.props[ARCANA_SAC_KEY].get_vector();
    ASSERT(current_arcane_sacrifices.empty());

    for (const vector<mutation_type> arcane_sacrifice_list :
                                    _arcane_sacrifice_lists)
    {
        const mutation_type sacrifice =
            _random_valid_sacrifice(arcane_sacrifice_list);

        if (sacrifice == MUT_NON_MUTATION)
            return;  // don't bother filling out the others, we failed
        current_arcane_sacrifices.push_back(sacrifice);
    }

    ASSERT(current_arcane_sacrifices.size()
           == ARRAYSZ(_arcane_sacrifice_lists));
}

/**
 * Has the player sacrificed any arcana?
 */
static bool _player_sacrificed_arcana()
{
    for (const vector<mutation_type> arcane_sacrifice_list :
                                    _arcane_sacrifice_lists)
    {
        for (mutation_type sacrifice : arcane_sacrifice_list)
            if (player_mutation_level(sacrifice))
                return true;
    }
    return false;
}

/**
 * Is the given sacrifice a valid one for Ru to offer to the player right now?
 *
 * @param sacrifice     The sacrifice in question.
 * @return              Whether Ru can offer the player that sacrifice, or
 *                      whether something is blocking it (e.g. no sacrificing
 *                      armour for races that can't wear any...)
 */
static bool _sacrifice_is_possible(sacrifice_def &sacrifice)
{
    if (sacrifice.mutation != MUT_NON_MUTATION
        && player_mutation_level(sacrifice.mutation))
    {
        return false;
    }

    if (sacrifice.sacrifice_vector)
    {
        const char* key = sacrifice.sacrifice_vector;
        // XXX: changing state in this function seems sketchy
        if (sacrifice.sacrifice == ABIL_RU_SACRIFICE_ARCANA)
            _choose_arcana_mutations();
        else
            _choose_sacrifice_mutation(sacrifice.sacrifice_vector);

        if (you.props[key].get_vector().empty())
            return false;
    }

    if (sacrifice.valid != nullptr && !sacrifice.valid())
        return false;

    return true;
}

/**
 * Which sacrifices are valid for Ru to potentially present to the player?
 *
 * @return      A list of potential sacrifices (e.g. ABIL_RU_SACRIFICE_WORDS).
 */
static vector<ability_type> _get_possible_sacrifices()
{
    vector<ability_type> possible_sacrifices;

    for (auto sacrifice : sac_data)
        if (_sacrifice_is_possible(sacrifice))
            possible_sacrifices.push_back(sacrifice.sacrifice);

    return possible_sacrifices;
}

/**
 * What's the name of the spell school corresponding to the given Ru mutation?
 *
 * @param mutation  The variety of MUT_NO_*_MAGIC in question.
 * @return          A long school name ("Summoning", "Translocations", etc.)
 */
static const char* _arcane_mutation_to_school_name(mutation_type mutation)
{
    // XXX: this does a really silly dance back and forth between school &
    // spelltype.
    const skill_type sk = arcane_mutation_to_skill(mutation);
    const spschool_flag_type school = skill2spell_type(sk);
    return spelltype_long_name(school);
}

/**
 * What's the abbreviation of the spell school corresponding to the given Ru
 * mutation?
 *
 * @param mutation  The variety of MUT_NO_*_MAGIC in question.
 * @return          A school abbreviation ("Summ", "Tloc", etc.)
 */
static const char* _arcane_mutation_to_school_abbr(mutation_type mutation)
{
    // XXX: this does a really silly dance back and forth between school &
    // spelltype.
    const auto school = skill2spell_type(arcane_mutation_to_skill(mutation));
    return spelltype_short_name(school);
}

static int _piety_for_skill(skill_type skill)
{
    return skill_exp_needed(you.skills[skill], skill, you.species) / 500;
}

static int _piety_for_skill_by_sacrifice(ability_type sacrifice)
{
    int piety_gain = 0;
    const sacrifice_def &sac_def = _get_sacrifice_def(sacrifice);

    piety_gain += _piety_for_skill(sac_def.sacrifice_skill);
    if (sacrifice == ABIL_RU_SACRIFICE_HAND)
    {
        // No one-handed staves for small races.
        if (species_size(you.species, PSIZE_TORSO) <= SIZE_SMALL)
            piety_gain += _piety_for_skill(SK_STAVES);
        // No one-handed bows.
        if (you.species != SP_FORMICID)
            piety_gain += _piety_for_skill(SK_BOWS);
    }
    return piety_gain;
}

#define AS_MUT(csv) (static_cast<mutation_type>((csv).get_int()))

/**
 * Adjust piety based on stat ranking. You get less piety if you're looking at
 * your lower stats.
 *
 * @param stat_type input_stat The stat we're checking.
 * @param int       multiplier How much piety for each rank position off.
 * @return          The piety to add.
 */
static int _get_stat_piety(stat_type input_stat, int multiplier)
{
    int stat_val = 3; // If this is your highest stat.
    if (you.base_stats[STAT_INT] > you.base_stats[input_stat])
            stat_val -= 1;
    if (you.base_stats[STAT_STR] > you.base_stats[input_stat])
            stat_val -= 1;
    if (you.base_stats[STAT_DEX] > you.base_stats[input_stat])
            stat_val -= 1;
    return stat_val * multiplier;
}

int get_sacrifice_piety(ability_type sac, bool include_skill)
{
    if (sac == ABIL_RU_REJECT_SACRIFICES)
        return INT_MAX; // used as the null sacrifice

    const sacrifice_def &sac_def = _get_sacrifice_def(sac);
    int piety_gain = sac_def.base_piety;
    ability_type sacrifice = sac_def.sacrifice;
    mutation_type mut = MUT_NON_MUTATION;
    int num_sacrifices = 0;

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
            arcane_skill = arcane_mutation_to_skill(arcane_mut);
            piety_gain += _piety_for_skill(arcane_skill);
        }
    }
    else if (sac_def.sacrifice_skill != SK_NONE && include_skill)
        piety_gain += _piety_for_skill_by_sacrifice(sac_def.sacrifice);

    switch (sacrifice)
    {
        case ABIL_RU_SACRIFICE_ESSENCE:
            if (mut == MUT_LOW_MAGIC)
            {
                piety_gain += 10 + max(you.skill_rdiv(SK_INVOCATIONS, 1, 2),
                                       max( you.skill_rdiv(SK_SPELLCASTING, 1, 2),
                                            you.skill_rdiv(SK_EVOCATIONS, 1, 2)));
            }
            else if (mut == MUT_MAGICAL_VULNERABILITY)
                piety_gain += 28;
            else
                piety_gain += 2 + _get_stat_piety(STAT_INT, 6);
            break;
        case ABIL_RU_SACRIFICE_PURITY:
            if (mut == MUT_WEAK || mut == MUT_DOPEY || mut == MUT_CLUMSY)
            {
                const stat_type stat = mut == MUT_WEAK   ? STAT_STR
                                     : mut == MUT_CLUMSY ? STAT_DEX
                                     : mut == MUT_DOPEY  ? STAT_INT
                                                         : NUM_STATS;
                piety_gain += 4 + _get_stat_piety(stat, 4);
            }
            // the other sacrifices get sharply worse if you already
            // have levels of them.
            else if (player_mutation_level(mut) == 2)
                piety_gain += 28;
            else if (player_mutation_level(mut) == 1)
                piety_gain += 21;
            else
                piety_gain += 14;
            break;
        case ABIL_RU_SACRIFICE_ARTIFICE:
            if (player_mutation_level(MUT_NO_LOVE))
                piety_gain -= 10; // You've already lost some value here
            break;
        case ABIL_RU_SACRIFICE_NIMBLENESS:
            if (player_mutation_level(MUT_NO_ARMOUR))
                piety_gain += 20;
            else if (species_apt(SK_ARMOUR) == UNUSABLE_SKILL)
                piety_gain += 28; // this sacrifice is worse for these races
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
        case ABIL_RU_SACRIFICE_EXPERIENCE:
            if (player_mutation_level(MUT_COWARDICE))
                piety_gain += 15;
        case ABIL_RU_SACRIFICE_COURAGE:
            if (player_mutation_level(MUT_INEXPERIENCED))
                piety_gain += 15;

        default:
            break;
    }

    // Award piety for any mutations removed by adding new innate muts
    // These can only be removed positive mutations, so we'll always give piety.
    if (sacrifice == ABIL_RU_SACRIFICE_PURITY
        || sacrifice == ABIL_RU_SACRIFICE_HEALTH
        || sacrifice == ABIL_RU_SACRIFICE_ESSENCE)
    {
        piety_gain *= 1 + mut_check_conflict(mut);
    }

    // Randomize piety gain very slightly to prevent counting.
    // We fuzz the piety gain by up to +-10%, or 5 piety, whichever is smaller.
    int piety_blur_inc = min(5, piety_gain / 10);
    int piety_blur = random2((2 * piety_blur_inc) + 1) - piety_blur_inc;

    return piety_gain + piety_blur;
}

// Remove the offer of sacrifices after they've been offered for sufficient
// time or it's time to offer something new.
static void _ru_expire_sacrifices()
{
    static const char *sacrifice_keys[] =
    {
        AVAILABLE_SAC_KEY,
        ESSENCE_SAC_KEY,
        HEALTH_SAC_KEY,
        PURITY_SAC_KEY,
        ARCANA_SAC_KEY,
    };

    for (auto key : sacrifice_keys)
    {
        ASSERT(you.props.exists(key));
        you.props[key].get_vector().clear();
    }

    // Clear out stored sacrfiice values.
    for (int i = 0; i < NUM_ABILITIES; ++i)
        you.sacrifice_piety[i] = 0;
}

/**
 * Choose a random sacrifice from those in the list, filtering to only those
 * with piety values <= the given cap.
 *
 * @param possible_sacrifices   The list of sacrifices to choose from.
 * @param min_piety             The maximum sac piety cost to accept.
 * @return                      The ability_type of a valid sacrifice, or
 *                              ABIL_RU_REJECT_SACRIFICES if none were found
 *                              (should never happen!)
 */
static ability_type _random_cheap_sacrifice(
        const vector<ability_type> &possible_sacrifices,
                                            int piety_cap)
{
    // XXX: replace this with random_if when that's merged
    ability_type chosen_sacrifice = ABIL_RU_REJECT_SACRIFICES;
    int valid_sacrifices = 0;
    for (auto sacrifice : possible_sacrifices)
    {
        if (get_sacrifice_piety(sacrifice) + you.piety > piety_cap)
            continue;

        ++valid_sacrifices;
        if (one_chance_in(valid_sacrifices))
            chosen_sacrifice = sacrifice;
    }

    dprf("found %d valid sacrifices; chose %d",
         valid_sacrifices, chosen_sacrifice);

    return chosen_sacrifice;
}

/**
 * Choose the cheapest remaining sacrifice. This is used when the cheapest
 * remaining sacrifice is over the piety cap and we still need to fill out 3
 * options.
 *
 * @param possible_sacrifices   The list of sacrifices to choose from.
 * @return                      The ability_type of the cheapest remaining
 *                              sacrifice.
 */
static ability_type _get_cheapest_sacrifice(
        const vector<ability_type> &possible_sacrifices)
{
    // XXX: replace this with random_if when that's merged
    ability_type chosen_sacrifice = ABIL_RU_REJECT_SACRIFICES;
    int last_piety = 999;
    int cheapest_sacrifices = 0;
    for (auto sacrifice : possible_sacrifices)
    {
        int sac_piety = get_sacrifice_piety(sacrifice);
        if (sac_piety >= last_piety)
            continue;

        ++cheapest_sacrifices;
        if (one_chance_in(cheapest_sacrifices))
        {
            chosen_sacrifice = sacrifice;
            last_piety = sac_piety;
        }
    }

    dprf("found %d cheapest sacrifices; chose %d",
         cheapest_sacrifices, chosen_sacrifice);

    return chosen_sacrifice;
}

/**
 * Chooses three distinct sacrifices to offer the player, store them in
 * available_sacrifices, and print a message to the player letting them
 * know that their new sacrifices are ready.
 */
void ru_offer_new_sacrifices()
{
    _ru_expire_sacrifices();

    vector<ability_type> possible_sacrifices = _get_possible_sacrifices();

    // for now we'll just pick three at random
    int num_sacrifices = possible_sacrifices.size();

    const int num_expected_offers = 3;

    // This can't happen outside wizmode, but may as well handle gracefully
    if (num_sacrifices < num_expected_offers)
        return;

    ASSERT(you.props.exists(AVAILABLE_SAC_KEY));
    CrawlVector &available_sacrifices
        = you.props[AVAILABLE_SAC_KEY].get_vector();

    for (int sac_num = 0; sac_num < num_expected_offers; ++sac_num)
    {
        // find the cheapest available sacrifice, in case we're close to ru's
        // max piety. (minimize 'wasted' piety in those cases.)
        const ability_type min_piety_sacrifice
            = accumulate(possible_sacrifices.begin(),
                         possible_sacrifices.end(),
                         ABIL_RU_REJECT_SACRIFICES,
                         [](ability_type a, ability_type b) {
                             return get_sacrifice_piety(a)
                                  < get_sacrifice_piety(b) ? a : b;
                         });
        const int min_piety = get_sacrifice_piety(min_piety_sacrifice);
        const int piety_cap = max(179, you.piety + min_piety);

        dprf("cheapest sac %d (%d piety); cap %d",
             min_piety_sacrifice, min_piety, piety_cap);

        // XXX: replace this with random_if when that's merged
        ability_type chosen_sacrifice
            = _random_cheap_sacrifice(possible_sacrifices, piety_cap);

        if (chosen_sacrifice < ABIL_FIRST_SACRIFICE ||
                chosen_sacrifice > ABIL_FINAL_SACRIFICE)
        {
           chosen_sacrifice = _get_cheapest_sacrifice(possible_sacrifices);
        }

        if (chosen_sacrifice > ABIL_FINAL_SACRIFICE)
        {
            // We don't have three sacrifices to offer for some reason.
            // Either the player is messing around in wizmode or has rejoined
            // Ru repeatedly. In either case, we'll just stop offering
            // sacrifices rather than crashing.
            _ru_expire_sacrifices();
            ru_reset_sacrifice_timer(false);
            return;
        }

        // add it to the list of chosen sacrifices to offer, and remove it from
        // the list of possibilities for the later sacrifices
        available_sacrifices.push_back(chosen_sacrifice);
        you.sacrifice_piety[chosen_sacrifice] =
                                get_sacrifice_piety(chosen_sacrifice, false);
        possible_sacrifices.erase(remove(possible_sacrifices.begin(),
                                         possible_sacrifices.end(),
                                         chosen_sacrifice),
                                  possible_sacrifices.end());
    }

    simple_god_message(" believes you are ready to make a new sacrifice.");
    // included in default force_more_message
}

/// What key corresponds to the potential/chosen mut(s) for this sacrifice?
string ru_sacrifice_vector(ability_type sac)
{
    const sacrifice_def &sac_def = _get_sacrifice_def(sac);
    return sac_def.sacrifice_vector ? sac_def.sacrifice_vector : "";
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

static const string _piety_asterisks(int piety)
{
    const int prank = piety_rank(piety);
    return string(prank, '*') + string(NUM_PIETY_STARS - prank, '.');
}

static void _apply_ru_sacrifice(mutation_type sacrifice)
{
    perma_mutate(sacrifice, 1, "Ru sacrifice");
    you.sacrifices[sacrifice] += 1;
}

static bool _execute_sacrifice(ability_type sac, const char* message)
{
    mprf("Ru asks you to %s.", message);
    mpr(ru_sacrifice_description(sac));
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
    you.can_train.set(skill, false);
    reset_training();
    check_selected_skills();
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
        if (shield != nullptr)
        {
            mprf("You can no longer hold %s!",
                shield->name(DESC_YOUR).c_str());
            unequip_item(EQ_SHIELD);
        }

        // And your two-handed weapon
        if (weapon != nullptr)
        {
            if (you.hands_reqd(*weapon) == HANDS_TWO)
            {
                mprf("You can no longer hold %s!",
                    weapon->name(DESC_YOUR).c_str());
                unequip_item(EQ_WEAPON);
            }
        }

        // And one ring
        if (ring != nullptr)
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
    else if (sac_def.sacrifice == ABIL_RU_SACRIFICE_EXPERIENCE)
        adjust_level(-RU_SAC_XP_LEVELS);
    else if (sac_def.sacrifice == ABIL_RU_SACRIFICE_SKILL)
    {
        uint8_t saved_skills[NUM_SKILLS];
        for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        {
            saved_skills[sk] = you.skills[sk];
            check_skill_level_change(sk, false);
        }

        // Produce messages about skill increases/decreases. We
        // restore one skill level at a time so that at most the
        // skill being checked is at the wrong level.
        for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        {
            you.skills[sk] = saved_skills[sk];
            check_skill_level_change(sk);
        }

        redraw_screen();
    }
}

/**
 * Describe variable costs for a given Ru sacrifice being offered.
 *
 * @param sac       The sacrifice in question.
 * @return          Extra costs.
 *                  For ABIL_RU_SACRIFICE_ARCANA: e.g. " (Tloc/Air/Fire)"
 *                  For other variable muts: e.g. " (frail)"
 *                  Otherwise, "".
 */
string ru_sac_text(ability_type sac)
{
    const sacrifice_def &sac_def = _get_sacrifice_def(sac);
    if (!sac_def.sacrifice_vector)
        return "";

    ASSERT(you.props.exists(sac_def.sacrifice_vector));
    const CrawlVector &sacrifice_muts =
        you.props[sac_def.sacrifice_vector].get_vector();

    if (sac != ABIL_RU_SACRIFICE_ARCANA)
    {
        ASSERT(sacrifice_muts.size() == 1);
        const mutation_type mut = AS_MUT(sacrifice_muts[0]);
        return make_stringf(" (%s)", mutation_name(mut));
    }

    // "Tloc/Fire/Ice"
    const string school_names
        = comma_separated_fn(sacrifice_muts.begin(), sacrifice_muts.end(),
                [](CrawlStoreValue mut) {
                    return _arcane_mutation_to_school_abbr(AS_MUT(mut));
                }, "/", "/");

    return make_stringf(" (%s)", school_names.c_str());
}

static int _ru_get_sac_piety_gain(ability_type sac)
{
    const sacrifice_def &sac_def = _get_sacrifice_def(sac);

    // If we're haven't yet calculated piety gain, get it now. Otherwise,
    // use the calculated value and then add in the skill piety, which isn't
    // saved because it can change over time.
    const int base_piety_gain = you.sacrifice_piety[sac];

    if (base_piety_gain == 0)
        return get_sacrifice_piety(sac);

    if (sac_def.sacrifice_skill != SK_NONE)
        return base_piety_gain + _piety_for_skill_by_sacrifice(sac);

    return base_piety_gain;
}

string ru_sacrifice_description(ability_type sac)
{
    const int piety_gain = _ru_get_sac_piety_gain(sac);
    return make_stringf("This is %s sacrifice. Piety after sacrifice: %s",
                        _describe_sacrifice_piety_gain(piety_gain),
                        _piety_asterisks(you.piety + piety_gain).c_str());
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
                sac_text = mut_upgrade_summary(mut);
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
        string handtxt = "";
        if (sac == ABIL_RU_SACRIFICE_HAND)
            handtxt = you.hand_name(true);

        offer_text = sac_def.sacrifice_text + handtxt;
        mile_text = make_stringf("%s.", sac_def.milestone_text);
    }

    // get confirmation that the sacrifice is desired.
    if (!_execute_sacrifice(sac, offer_text.c_str()))
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
            arcane_skill = arcane_mutation_to_skill(arcane_mut);
            _ru_kill_skill(arcane_skill);
        }
    }
    else if (sac_def.sacrifice_skill != SK_NONE)
        _ru_kill_skill(sac_def.sacrifice_skill);

    // Maybe this should go in _extra_sacrifice_code, but it would be
    // inconsistent for the milestone to have reduced Shields skill
    // but not the others.
    if (sac == ABIL_RU_SACRIFICE_HAND)
    {
        // No one-handed staves for small races.
        if (species_size(you.species, PSIZE_TORSO) <= SIZE_SMALL)
            _ru_kill_skill(SK_STAVES);
        // No one-handed bows.
        if (you.species != SP_FORMICID)
            _ru_kill_skill(SK_BOWS);
    }

    mark_milestone("sacrifice", mile_text);

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

    // Actually give the piety for this sacrifice.
    set_piety(min(piety_breakpoint(5),
                  you.piety + _ru_get_sac_piety_gain(sac)));

    if (you.piety == piety_breakpoint(5))
        simple_god_message(" indicates that your awakening is complete.");

    // Clean up.
    _ru_expire_sacrifices();
    ru_reset_sacrifice_timer(true);
    redraw_screen(); // pretty much everything could have changed
    return true;
}

/**
 * If forced_rejection is false, prompt the player if they want to reject the
 * currently offered sacrifices. If true, or if the prompt is accepted,
 * remove the currently offered sacrifices & increase the time until the next
 * sacrifices will be offered.
 *
 * @param forced_rejection      Whether the rejection is caused by the removal
 *                              of an amulet of faith, in which case there's
 *                              no prompt & an increased sac time penalty.
 * @return                      Whether the sacrifices were actually rejected.
 */
bool ru_reject_sacrifices(bool forced_rejection)
{
    if (!forced_rejection &&
        !yesno("Do you really want to reject the sacrifices Ru is offering?",
               false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    ru_reset_sacrifice_timer(false, true);
    _ru_expire_sacrifices();
    simple_god_message(" will take longer to evaluate your readiness.");
    return true;
}

/**
 * Reset the time until the next set of Ru sacrifices are offered.
 *
 * @param clear_timer       Whether to reset the timer to the base time-between-
 *                          sacrifices, rather than adding to it.
 * @param faith_penalty     Whether this is a penalty for removing "faith.
 */
void ru_reset_sacrifice_timer(bool clear_timer, bool faith_penalty)
{
    ASSERT(you.props.exists(RU_SACRIFICE_PROGRESS_KEY));
    ASSERT(you.props.exists(RU_SACRIFICE_DELAY_KEY));
    ASSERT(you.props.exists(RU_SACRIFICE_PENALTY_KEY));

    // raise the delay if there's an active sacrifice, and more so the more
    // often you pass on a sacrifice and the more piety you have.
    const int base_delay = 80;
    int delay = you.props[RU_SACRIFICE_DELAY_KEY].get_int();
    int added_delay;
    if (clear_timer)
    {
        added_delay = 0;
        delay = base_delay;
        you.props[RU_SACRIFICE_PENALTY_KEY] = 0;
    }
    else
    {
        // if you rejected a sacrifice, add between 33 and 53 to the timer,
        // based on piety. This extra delay stacks with any added delay for
        // previous rejections.
        added_delay = you.props[RU_SACRIFICE_PENALTY_KEY].get_int();
        const int new_penalty = (max(100, static_cast<int>(you.piety))) / 3;
        added_delay += new_penalty;

        // longer delay for each real rejection
        if (!you.props[AVAILABLE_SAC_KEY].get_vector().empty())
            you.props[RU_SACRIFICE_PENALTY_KEY] = added_delay;

        if (faith_penalty)
        {
            // the player will end up waiting longer than they would otherwise,
            // but multiple removals of the amulet of faith won't stack -
            // they'll just put the player back to around the same delay
            // each time.
            added_delay += new_penalty * 2;
            delay = base_delay;
        }
    }

    delay = div_rand_round((delay + added_delay) * (3 + you.faith()), 3);
    if (crawl_state.game_is_sprint())
        delay /= SPRINT_MULTIPLIER;

    you.props[RU_SACRIFICE_DELAY_KEY] = delay;
    you.props[RU_SACRIFICE_PROGRESS_KEY] = 0;
}

// Check to see if you're eligible to retaliate.
//Your chance of eligiblity scales with piety.
bool will_ru_retaliate()
{
    // Scales up to a 25% chance of retribution
    return have_passive(passive_t::upgraded_aura_of_power)
           && crawl_state.which_god_acting() != GOD_RU
           && one_chance_in(div_rand_round(640, you.piety));
}

// Power of retribution increases with damage, decreases with monster HD.
void ru_do_retribution(monster* mons, int damage)
{
    int power = max(0, random2(div_rand_round(you.piety*10, 32))
        + damage - (2 * mons->get_hit_dice()));
    const actor* act = &you;

    if (power > 50 && (mons->antimagic_susceptible()))
    {
        mprf(MSGCH_GOD, "You focus your will and drain %s's magic in "
                "retribution!", mons->name(DESC_THE).c_str());
        mons->add_ench(mon_enchant(ENCH_ANTIMAGIC, 1, act, power+random2(320)));
    }
    else if (power > 35)
    {
        mprf(MSGCH_GOD, "You focus your will and paralyse %s in retribution!",
                mons->name(DESC_THE).c_str());
        mons->add_ench(mon_enchant(ENCH_PARALYSIS, 1, act, power+random2(60)));
    }
    else if (power > 25)
    {
        mprf(MSGCH_GOD, "You focus your will and slow %s in retribution!",
                mons->name(DESC_THE).c_str());
        mons->add_ench(mon_enchant(ENCH_SLOW, 1, act, power+random2(100)));
    }
    else if (power > 10 && mons_can_be_blinded(mons->type))
    {
        mprf(MSGCH_GOD, "You focus your will and blind %s in retribution!",
                mons->name(DESC_THE).c_str());
        mons->add_ench(mon_enchant(ENCH_BLIND, 1, act, power+random2(100)));
    }
    else if (power > 0)
    {
        mprf(MSGCH_GOD, "You focus your will and illuminate %s in retribution!",
                mons->name(DESC_THE).c_str());
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
        trap_def *trap = trap_at(you.pos());
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
    drain_player(30, false, true);
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
    dist beam;

    while (1)
    {
        direction_chooser_args args;
        args.restricts = DIR_LEAP;
        args.mode = TARG_ANY;
        args.range = 3;
        args.needs_path = false;
        args.top_prompt = "Aiming: <white>Power Leap</white>";
        args.self = CONFIRM_CANCEL;
        const int explosion_size = 1;
        targetter_smite tgt(&you, args.range, explosion_size, explosion_size);
        args.hitfunc = &tgt;
        direction(beam, args);
        if (crawl_state.seen_hups)
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
        if (mons && you.can_see(*mons))
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
            canned_msg(MSG_SOMETHING_IN_WAY);
        }
        else
        {
            clear_messages();
            canned_msg(MSG_CANNOT_SEE);
        }
    }

    if (you.attempt_escape(2)) // returns true if not constricted
    {
        if (cell_is_solid(beam.target) || monster_at(beam.target))
            mpr("Something unexpectedly blocked you, preventing you from leaping!");
        else
            move_player_to_grid(beam.target, false);
    }

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

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
        if (mon == nullptr || mons_is_projectile(mon->type) || mon->friendly())
            continue;
        ASSERT(mon);

        //damage scales with XL amd piety
        mon->hurt((actor*)&you, roll_dice(1 + div_rand_round(you.piety *
            (54 + you.experience_level), 777), 3),
            BEAM_ENERGY, KILLED_BY_BEAM, "", "", true);
    }

    return true;
}

int cell_has_valid_target(coord_def where)
{
    monster* mon = monster_at(where);
    if (mon == nullptr || mons_is_projectile(mon->type) || mon->friendly())
        return 0;
    return 1;
}

static int _apply_apocalypse(coord_def where)
{
    if (!cell_has_valid_target(where))
        return 0;
    monster* mons = monster_at(where);
    ASSERT(mons);

    int duration = 0;
    string message = "";
    enchant_type enchantment = ENCH_NONE;

    int effect = random2(4);
    if (mons_is_firewood(*mons))
        effect = 99; // > 2 is just damage -- no slowed toadstools

    int num_dice;
    switch (effect)
    {
        case 0:
            if (mons->antimagic_susceptible())
            {
                message = " loses " + mons->pronoun(PRONOUN_POSSESSIVE)
                          + " magic into the devouring truth!";
                enchantment = ENCH_ANTIMAGIC;
                duration = 500 + random2(200);
                num_dice = 4;
                break;
            } // if not antimagicable, fall through to paralysis.
        case 1:
            message = " is paralysed by terrible understanding!";
            enchantment = ENCH_PARALYSIS;
            duration = 80 + random2(60);
            num_dice = 4;
            break;

        case 2:
            message = " slows down under the weight of truth!";
            enchantment = ENCH_SLOW;
            duration = 300 + random2(100);
            num_dice = 6;
            break;

        default:
            num_dice = 8;
            break;
    }

    //damage scales with XL and piety
    const int pow = you.piety;
    int die_size = 1 + div_rand_round(pow * (54 + you.experience_level), 584);
    int dmg = 10 + roll_dice(num_dice, die_size);

    mons->hurt(&you, dmg, BEAM_ENERGY, KILLED_BY_BEAM, "", "", true);

    if (mons->alive() && enchantment != ENCH_NONE)
    {
        simple_monster_message(*mons, message.c_str());
        mons->add_ench(mon_enchant(enchantment, 1, &you, duration));
    }
    return 1;
}

bool ru_apocalypse()
{
    int count = apply_area_visible(cell_has_valid_target, you.pos());
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
    apply_area_visible(_apply_apocalypse, you.pos());
    drain_player(100, false, true);
    return true;
}

bool pakellas_check_quick_charge(bool quiet)
{
    if (!enough_mp(1, quiet))
        return false;

    if (!any_items_of_type(OSEL_DIVINE_RECHARGE))
    {
        if (!quiet)
            mpr(no_selectables_message(OSEL_DIVINE_RECHARGE));
        return false;
    }

    return true;
}

/**
 * Calculate the effective power of a surged hex wand.
 * Works by iterating over the possible rolls from random2avg().
 * A much nicer way of computing this would be appreciated.
 *
 * @param   pow The base power.
 * @returns     The effective spellpower of a hex wand.
 */
int pakellas_effective_hex_power(int pow)
{
    if (!you_worship(GOD_PAKELLAS) || !you.duration[DUR_DEVICE_SURGE])
        return pow;

    if (you.magic_points == 0)
        return 0;

    const int die_size = you.piety * 9 / piety_breakpoint(5);
    const int max_roll = max(3, 2 * die_size);
    vector<int> rolls(max_roll + 1, 0);

    // i is the first random2(); j is the second random2()
    int i = 0;
    for (; i < die_size; i++)
        for (int j = 0; j < die_size; j++)
        {
            // This should be the same as the formula in pakellas_device_surge()
            int roll = min(you.magic_points,
                              min(9,
                                  max(3,
                                      1 + (i + j) / 2)));
            rolls[roll] = rolls[roll] + 1;
        }

    if (die_size == 0)
        rolls[min(3, you.magic_points)] = 1;

    int total_pow = 0;
    int weight = 0;

    for (i = 1; i <= max_roll; i++)
    {
        if (i > 9)
            break;

        int base_sev = i / 3;
        int mod = i % 3;
        int base_power = (base_sev == 0)
            ? 0 // fizzle
            : stepdown_spellpower(100*apply_enhancement(pow, base_sev));
        weight += 3 * rolls[i];
        total_pow +=
            rolls[i] *
            (base_power * (3 - mod)
             + stepdown_spellpower(100*apply_enhancement(pow, base_sev+1))
               * mod);
    }
    total_pow /= weight;
    return total_pow;
}

/**
 * Trigger a readied Device Surge, spending MP to multiply evocations power.
 *
 * @return  A number of enhancers (!) to multiply evo power by.
 */
int pakellas_surge_devices()
{
    if (!you_worship(GOD_PAKELLAS) || !you.duration[DUR_DEVICE_SURGE])
        return 0;

    const int mp = min(you.magic_points, min(9, max(3,
                       1 + random2avg(you.piety * 9 / piety_breakpoint(5),
                                      2))));

    const int severity = div_rand_round(mp, 3);
    dec_mp(mp);
    you.duration[DUR_DEVICE_SURGE] = 0;
    if (severity == 0)
    {
        mprf(MSGCH_GOD, "The surge fizzles.");
        return -1;
    }
    return severity;
}

static bool _get_stomped(monster& mons)
{
    // Don't hurt your own demonic guardians
    if (testbits(mons.flags, MF_DEMONIC_GUARDIAN) && mons.friendly())
        return false;

    behaviour_event(&mons, ME_ANNOY, &you);

    // Damage starts at 1/6th of monster current HP, then gets some damage
    // scaling off Invo power.
    int damage = div_rand_round(mons.hit_points, 6);
    int die_size = 2 + div_rand_round(you.skill(SK_INVOCATIONS), 2);
    damage += roll_dice(2, die_size);

    mons.hurt(&you, damage, BEAM_ENERGY, KILLED_BY_BEAM, "", "", true);

    if (mons.alive() && you.can_see(mons))
        print_wounds(mons);

    return true;
}

bool uskayaw_stomp()
{
    mpr("You stomp with the beat, sending a shockwave through the revelers "
            "around you!");
    apply_monsters_around_square(_get_stomped, you.pos());
    return true;
}

bool uskayaw_line_pass()
{
    ASSERT(!crawl_state.game_is_arena());

    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("You can't repeat line pass.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return false;
    }

    // query for location:
    int range = 8;
    int invo_skill = you.skill(SK_INVOCATIONS);
    int pow = (25 + invo_skill + random2(invo_skill));
    dist beam;
    bolt line_pass;
    line_pass.thrower = KILL_YOU;
    line_pass.name = "line pass";
    line_pass.source_name = "you";
    line_pass.source_id = MID_PLAYER;
    line_pass.flavour = BEAM_IRRESISTIBLE_CONFUSION;
    line_pass.source = you.pos();
    line_pass.hit = AUTOMATIC_HIT;
    line_pass.range = range;
    line_pass.ench_power = pow;
    line_pass.pierce = true;

    while (1)
    {
        unique_ptr<targetter> hitfunc;
        hitfunc = make_unique<targetter_monster_sequence>(&you, pow, range);

        direction_chooser_args args;
        args.hitfunc = hitfunc.get();
        args.restricts = DIR_LEAP;
        args.mode = TARG_ANY;
        args.needs_path = false;
        args.top_prompt = "Aiming: <white>Line Pass</white>";
        args.range = 8;

        if (!spell_direction(beam, line_pass, &args))
            return SPRET_ABORT;

        if (crawl_state.seen_hups)
        {
            clear_messages();
            mpr("Cancelling line pass due to HUP.");
            return false;
        }

        if (!beam.isValid || beam.target == you.pos())
            return false;         // early return

        monster* beholder = you.get_beholder(beam.target);
        if (beholder)
        {
            clear_messages();
            mprf("You cannot move away from %s!",
                 beholder->name(DESC_THE, true).c_str());
            continue;
        }

        monster* fearmonger = you.get_fearmonger(beam.target);
        if (fearmonger)
        {
            clear_messages();
            mprf("You cannot move closer to %s!",
                 fearmonger->name(DESC_THE, true).c_str());
            continue;
        }

        monster* mons = monster_at(beam.target);
        if (mons && you.can_see(*mons))
        {
            clear_messages();
            mpr("You can't stand on top of the monster!");
            continue;
        }

        if (grd(beam.target) == DNGN_OPEN_SEA)
        {
            clear_messages();
            mpr("You can't line pass into the sea!");
            continue;
        }
        else if (grd(beam.target) == DNGN_LAVA_SEA)
        {
            clear_messages();
            mpr("You can't line pass into the sea of lava!");
            continue;
        }
        else if (cell_is_solid(beam.target))
        {
            clear_messages();
            mpr("You can't walk through walls!");
            continue;
        }
        else if (!check_moveto(beam.target, "line pass"))
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
            canned_msg(MSG_SOMETHING_IN_WAY);
        }
        else
        {
            clear_messages();
            canned_msg(MSG_CANNOT_SEE);
        }
    }

    if (monster_at(beam.target))
        mpr("Something unexpectedly blocked you, preventing you from passing!");
    else
    {
        line_pass.fire();
        you.stop_being_constricted(false);
        move_player_to_grid(beam.target, false);
    }

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    return true;
}

bool uskayaw_grand_finale()
{
    ASSERT(!crawl_state.game_is_arena());

    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("No encores!");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return false;
    }

    // query for location:
    dist beam;

    monster* mons;

    while (1)
    {
        direction_chooser_args args;
        args.mode = TARG_HOSTILE;
        args.needs_path = false;
        args.top_prompt = "Aiming: <white>Grand Finale</white>";
        args.self = CONFIRM_CANCEL;
        targetter_smite tgt(&you, 7, 0, 0);
        args.hitfunc = &tgt;
        direction(beam, args);
        if (crawl_state.seen_hups)
        {
            clear_messages();
            mpr("Cancelling grand finale due to HUP.");
            return false;
        }

        if (!beam.isValid || beam.target == you.pos())
            return false;         // early return

        mons = monster_at(beam.target);
        if (!mons || !you.can_see(*mons))
        {
            clear_messages();
            mpr("You can't perceive a target there!");
            continue;
        }

        if (grd(beam.target) == DNGN_OPEN_SEA)
        {
            clear_messages();
            mpr("You would fall into the sea!");
            continue;
        }
        else if (grd(beam.target) == DNGN_LAVA_SEA)
        {
            clear_messages();
            mpr("You would fall into the sea of lava!");
            continue;
        }
        else if (!check_moveto(beam.target, "move"))
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
            canned_msg(MSG_SOMETHING_IN_WAY);
        }
        else
        {
            clear_messages();
            canned_msg(MSG_CANNOT_SEE);
        }
    }

    ASSERT(mons);

    // kill the target
    mprf("%s explodes violently!", mons->name(DESC_THE, false).c_str());
    mons->flags |= MF_EXPLODE_KILL;
    if (!mons->is_insubstantial()) {
        blood_spray(mons->pos(), mons->mons_species(), mons->hit_points / 5);
        throw_monster_bits(*mons); // have some fun while we're at it
    }

    monster_die(mons, KILL_YOU, NON_MONSTER, false);

    if (!mons->alive())
        move_player_to_grid(beam.target, false);
    else
        mpr("You spring back to your original position.");

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    set_piety(piety_breakpoint(0)); // Reset piety to 1*.

    return true;
}

/**
 * Permanently choose a class for the player's companion,
 * after prompting to make sure the player is certain.
 *
 * @param ancestor_choice     The ancestor's class; should be an ability enum.
 * @return                  Whether the player went through with the choice.
 */
bool hepliaklqana_choose_ancestor_type(int ancestor_choice)
{
    if (hepliaklqana_ancestor()
        && companion_is_elsewhere(hepliaklqana_ancestor()))
    {
        // ugly hack to avoid dealing with upgrading offlevel ancestors
        mpr("You can't make this choice while your ancestor is elsewhere.");
        return false;
    }

    static const map<int, monster_type> ancestor_types = {
        { ABIL_HEPLIAKLQANA_TYPE_KNIGHT, MONS_ANCESTOR_KNIGHT },
        { ABIL_HEPLIAKLQANA_TYPE_BATTLEMAGE, MONS_ANCESTOR_BATTLEMAGE },
        { ABIL_HEPLIAKLQANA_TYPE_HEXER, MONS_ANCESTOR_HEXER },
    };

    auto ancestor_mapped = map_find(ancestor_types, ancestor_choice);
    ASSERT(ancestor_mapped);
    const auto ancestor_type = *ancestor_mapped;
    const string ancestor_type_name = mons_type_name(ancestor_type, DESC_A);

    if (!yesno(make_stringf("Are you sure you want to remember your ancestor "
                            "as %s?", ancestor_type_name.c_str()).c_str(),
               false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    you.props[HEPLIAKLQANA_ALLY_TYPE_KEY] = ancestor_type;

    if (monster* ancestor = hepliaklqana_ancestor_mon())
    {
        ancestor->type = ancestor_type;
        give_weapon(ancestor, -1);
        ASSERT(ancestor->weapon());
        give_shield(ancestor);
        set_ancestor_spells(*ancestor);
    }

    god_speaks(you.religion, "It is so.");
    take_note(Note(NOTE_ANCESTOR_TYPE, 0, 0, ancestor_type_name));
    const string mile_text
        = make_stringf("remembered their ancestor %s as %s.",
                       hepliaklqana_ally_name().c_str(),
                       ancestor_type_name.c_str());
    mark_milestone("ancestor.class", mile_text);

    return true;
}


/**
 * Heal the player's ancestor, and apply the Idealised buff for a few turns.
 *
 * @param fail      Whether the effect should fail after checking validity.
 * @return          Whether the healing succeeded, failed, or was aborted.
 */
spret_type hepliaklqana_idealise(bool fail)
{
    const mid_t ancestor_mid = hepliaklqana_ancestor();
    if (ancestor_mid == MID_NOBODY)
    {
        mpr("You have no ancestor to preserve!");
        return SPRET_ABORT;
    }

    monster *ancestor = monster_by_mid(ancestor_mid);
    if (!ancestor || !you.can_see(*ancestor))
    {
        mprf("%s is not nearby!", hepliaklqana_ally_name().c_str());
        return SPRET_ABORT;
    }

    fail_check();

    simple_god_message(make_stringf(" grants %s healing and protection!",
                                    ancestor->name(DESC_YOUR).c_str()).c_str());

    // 1/3 mhp healed at 0 skill, full at 27 invo
    const int healing = ancestor->max_hit_points
                         * (9 + you.skill(SK_INVOCATIONS)) / 36;

    if (ancestor->heal(healing))
    {
        if (ancestor->hit_points == ancestor->max_hit_points)
            simple_monster_message(*ancestor, " is fully restored!");
        else
            simple_monster_message(*ancestor, " is healed somewhat.");
    }

    const int dur = random_range(50, 80)
                    + random2avg(you.skill(SK_INVOCATIONS, 20), 2);
    ancestor->add_ench({ ENCH_IDEALISED, 1, &you, dur});
    return SPRET_SUCCESS;
}

/**
 * Prompt to allow the player to choose a target for the Transference ability.
 *
 * @return  The chosen target, or the origin if none was chosen.
 */
static coord_def _get_transference_target()
{
    dist spd;

    const int aoe_radius = have_passive(passive_t::transfer_drain) ? 1 : 0;
    targetter_transference tgt(&you, aoe_radius);
    direction_chooser_args args;
    args.hitfunc = &tgt;
    args.restricts = DIR_TARGET;
    args.mode = TARG_MOBILE_MONSTER;
    args.range = LOS_RADIUS;
    args.needs_path = false;
    args.self = CONFIRM_NONE;
    args.show_floor_desc = true;
    args.top_prompt = "Select a target.";

    direction(spd, args);

    if (!spd.isValid)
        return coord_def();
    return spd.target;
}

/// Drain any monsters near the destination of Tranference.
static void _transfer_drain_nearby(coord_def destination)
{
    for (adjacent_iterator it(destination); it; ++it)
    {
        monster* mon = monster_at(*it);
        if (!mon || mons_is_hepliaklqana_ancestor(mon->type))
            continue;

        const int dur = random_range(60, 150);
        // 1-2 at 0 skill, 2-6 at 27 skill.
        const int degree
            = random_range(1 + you.skill_rdiv(SK_INVOCATIONS, 1, 27),
                           2 + you.skill_rdiv(SK_INVOCATIONS, 4, 27));
        if (mon->add_ench(mon_enchant(ENCH_DRAINED, degree, &you, dur)))
            simple_monster_message(*mon, " is drained by nostalgia.");
    }
}

/**
 * Activate Hepliaklqana's Transference ability, swapping the player's
 * ancestor with a targeted creature & potentially slowing monsters adjacent
 * to the target.
 *
 * @param fail      Whether the effect should fail after checking validity.
 * @return          Whether the ability succeeded, failed, or was aborted.
 */
spret_type hepliaklqana_transference(bool fail)
{
    monster *ancestor = hepliaklqana_ancestor_mon();
    if (!ancestor || !you.can_see(*ancestor))
    {
        mprf("%s is not nearby!", hepliaklqana_ally_name().c_str());
        return SPRET_ABORT;
    }

    coord_def target = _get_transference_target();
    if (target.origin())
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    actor* victim = actor_at(target);
    const bool victim_visible = victim && you.can_see(*victim);
    if ((!victim || !victim_visible)
        && !yesno("You can't see anything there. Try transferring anyway?",
                  true, 'n'))
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    if (victim == ancestor)
    {
        mpr("You can't transfer your ancestor with themself.");
        return SPRET_ABORT;
    }

    const bool victim_immovable
        = victim && (mons_is_tentacle_or_tentacle_segment(victim->type)
                     || victim->is_stationary());
    if (victim_visible && victim_immovable)
    {
        mpr("You can't transfer that.");
        return SPRET_ABORT;
    }

    const coord_def destination = ancestor->pos();
    if (victim == &you && !check_moveto(destination, "transfer"))
        return SPRET_ABORT;

    const bool uninhabitable = victim && !victim->is_habitable(destination);
    if (uninhabitable && victim_visible)
    {
        mprf("%s can't be transferred into %s.",
             victim->name(DESC_THE).c_str(), feat_type_name(grd(destination)));
        return SPRET_ABORT;
    }

    // we assume the ancestor flies & so can survive anywhere anything can.

    fail_check();

    if (!victim || uninhabitable || victim_immovable)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    if (victim->is_player())
    {
        ancestor->move_to_pos(target, true, true);
        victim->move_to_pos(destination, true, true);
    }
    else
        ancestor->swap_with(victim->as_monster());

    mprf("%s swap%s with %s!",
         victim->name(DESC_THE).c_str(),
         victim->is_player() ? "" : "s",
         ancestor->name(DESC_YOUR).c_str());

    check_place_cloud(CLOUD_MIST, target, random_range(10,20), ancestor);
    check_place_cloud(CLOUD_MIST, destination, random_range(10,20), ancestor);

    if (victim->is_monster())
        mons_relocated(victim->as_monster());

    ancestor->apply_location_effects(destination);
    victim->apply_location_effects(target);
    if (victim->is_monster())
        behaviour_event(victim->as_monster(), ME_DISTURB, &you, target);

    if (have_passive(passive_t::transfer_drain))
        _transfer_drain_nearby(target);

    return SPRET_SUCCESS;
}

/// Prompt to rename your ancestor.
static void _hepliaklqana_choose_name()
{
    const string old_name = hepliaklqana_ally_name();
    string prompt  = make_stringf("Remember %s name as what? ",
                                  apostrophise(old_name).c_str());

    char buf[18];
    int ret = msgwin_get_line(prompt, buf, sizeof buf, nullptr, old_name);
    if (ret)
    {
        canned_msg(MSG_OK);
        return;
    }

    // strip whitespace & colour tags
    const string new_name
        = trimmed_string(formatted_string::parse_string(buf).tostring());
    if (old_name == new_name || !new_name.size())
    {
        canned_msg(MSG_OK);
        return;
    }

    you.props[HEPLIAKLQANA_ALLY_NAME_KEY] = new_name;
    mprf("Yes, %s is definitely a better name.", new_name.c_str());
    upgrade_hepliaklqana_ancestor(true);
}

static void _hepliaklqana_choose_gender()
{
    static const string gender_names[] = { "neither", "male", "female" };
    const int current_gender
        = you.props[HEPLIAKLQANA_ALLY_GENDER_KEY].get_int();
    ASSERT(size_t(current_gender) < ARRAYSZ(gender_names));

    mprf(MSGCH_PROMPT,
         "Was %s a) male, b) female, or c) neither? (Currently %s.)",
         hepliaklqana_ally_name().c_str(),
         gender_names[current_gender].c_str());

    int keyin = toalower(get_ch());
    if (!isaalpha(keyin))
    {
        canned_msg(MSG_OK);
        return;
    }

    const uint32_t choice = keyin - 'a';
    if (choice > ARRAYSZ(gender_names))
    {
        canned_msg(MSG_OK);
        return;
    }

    // fun trick
    const int new_gender = (choice + 1) % 3;
    if (new_gender == current_gender)
    {
        canned_msg(MSG_OK);
        return;
    }

    you.props[HEPLIAKLQANA_ALLY_GENDER_KEY] = new_gender;
    mprf("%s was always %s, you're pretty sure.",
         hepliaklqana_ally_name().c_str(),
         gender_names[new_gender].c_str());
    upgrade_hepliaklqana_ancestor(true);
}

/// Rename and/or re-gender your ancestor.
void hepliaklqana_choose_identity()
{
    _hepliaklqana_choose_name();
    _hepliaklqana_choose_gender();
}

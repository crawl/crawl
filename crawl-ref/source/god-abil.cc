/**
 * @file
 * @brief God-granted abilities.
**/

#include "AppHdr.h"

#include "god-abil.h"

#include <cmath>
#include <numeric>
#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "attitude-change.h"
#include "bloodspatter.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "curse-type.h"
#include "dactions.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "dgn-overview.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "fight.h"
#include "files.h"
#include "fineff.h"
#include "format.h" // formatted_string
#include "god-blessing.h"
#include "god-companions.h"
#include "god-item.h"
#include "god-passive.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "losglobal.h"
#include "macro.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-gear.h" // H: give_weapon()/give_armour()
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "movement.h"
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
#include "skill-menu.h"
#include "spl-book.h"
#include "spl-goditem.h"
#include "spl-monench.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "sprint.h"
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "target.h"
#include "teleport.h" // monster_teleport
#include "terrain.h"
#ifdef USE_TILE
 #include "rltiles/tiledef-main.h"
#endif
#include "timed-effects.h"
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
    // Worshippers of Ignis can use their capstone with any amount of piety
    int pbreak = (god == GOD_IGNIS) ? -1 : 5;
    return in_good_standing(god, pbreak) && !you.one_time_ability_used[god];
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

    int item_slot = prompt_invent_item("Brand which weapon?",
                                       menu_type::invlist,
                                       OSEL_BLESSABLE_WEAPON, OPER_ANY,
                                       invprompt_flag::escape_only);

    if (item_slot == PROMPT_NOTHING || item_slot == PROMPT_ABORT)
    {
        canned_msg(MSG_OK);
        return false;
    }

    item_def& wpn(you.inv[item_slot]);
    // TSO and KIKU allow blessing ranged weapons, but LUGONU does not.
    if (!is_brandable_weapon(wpn, brand == SPWPN_HOLY_WRATH
                                                                  || brand == SPWPN_PAIN, true))
    {
        return false;
    }

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

    string old_name = wpn.name(DESC_A);
    set_equip_desc(wpn, ISFLAG_GLOWING);
    set_item_ego_type(wpn, OBJ_WEAPONS, brand);
    enchant_weapon(wpn, true);
    enchant_weapon(wpn, true);

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
        {
            if (is_bloodcovered(*ri))
                env.pgrid(*ri) &= ~FPROP_BLOODY;

            if (env.grid(*ri) == DNGN_FOUNTAIN_BLOOD)
                dungeon_terrain_changed(*ri, DNGN_FOUNTAIN_BLUE);
        }
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
        mpr("You have nothing to donate!");
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
        return true;
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
    "unbelievers",
    "heretics",
    "guilty",
    "hordes of the Abyss",
    "bastard children of Xom",
    "amorphous wretches",
    "fetid masses",
    "agents of filth",
    "squalid dregs",
    "legions of the damned",
    "servants of Hell",
    "forces of darkness",
};

// First column is adjective, then noun.
static const char * const sin_text[][2] =
{
    { "unfaithful",   "unfaithfulness" },
    { "disloyal",     "disloyalty" },
    { "doubting",     "doubt" },
    { "chaotic",      "chaos" },
    { "discordant",   "discord" },
    { "anarchic",     "anarchy" },
    { "unclean",      "uncleanliness" },
    { "impure",       "impurity" },
    { "contaminated", "contamination" },
    { "profane",      "profanity" },
    { "blasphemous",  "blasphemy" },
    { "sacrilegious", "sacrilege" },
};

// First column is adjective, then noun.
static const char * const virtue_text[][2] =
{
    { "faithful",  "faithfulness" },
    { "loyal",     "loyalty" },
    { "believing", "belief" },
    { "ordered",   "order" },
    { "harmonic",  "harmony" },
    { "lawful",    "lawfulness" },
    { "clean",     "cleanliness" },
    { "pure",      "purity" },
    { "hygienic",  "hygiene" },
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
        ostringstream out;
        out << bookname << ' ' << (chapter + 1) << ':' << (verse + 1);
        return out.str();
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
#else
    UNUSED(quiet);
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
    // Anything at or above (30+30)/2 = 30 'power' (HD) is completely immune.
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

    if (you.duration[DUR_RECITE_COOLDOWN])
    {
        if (!quiet)
            mpr("You're not ready to recite again yet.");
        return false;
    }

    return true;
}

vector<coord_def> find_recite_targets()
{
    vector<coord_def> result;
    recite_counts eligibility;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (you.can_see(**mi)
            && zin_check_recite_to_single_monster(*mi, eligibility,
                                                  true) == RE_ELIGIBLE)
        {
            result.push_back((*mi)->pos());
        }
    }

    return result;
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

enum class zin_eff
{
    nothing,
    daze,
    confuse,
    paralyse,
    smite,
    blind,
    silver_corona,
    antimagic,
    mute,
    mad,
    dumb,
    ignite_chaos,
    saltify,
    holy_word,
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
    zin_eff effect = zin_eff::nothing;

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
                effect = zin_eff::daze;
            else if (check < 10)
            {
                if (coinflip())
                    effect = zin_eff::confuse;
                else
                    effect = zin_eff::daze;
            }
            else if (check < 15)
                effect = zin_eff::confuse;
            else
            {
                if (one_chance_in(3))
                    effect = zin_eff::paralyse;
                else
                    effect = zin_eff::confuse;
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
                    effect = zin_eff::confuse;
                else
                    effect = zin_eff::smite;
            }
            else if (check < 10)
            {
                if (one_chance_in(3))
                    effect = zin_eff::blind;
                else if (mon->antimagic_susceptible())
                    effect = zin_eff::antimagic;
                else
                    effect = zin_eff::silver_corona;
            }
            else if (check < 15)
            {
                if (one_chance_in(3))
                    effect = zin_eff::blind;
                else if (coinflip())
                    effect = zin_eff::paralyse;
                else
                    effect = zin_eff::mute;
            }
            else
            {
                if (coinflip())
                    effect = zin_eff::mad;
                else
                    effect = zin_eff::dumb;
            }
        }
        break;

    case RECITE_CHAOTIC:
        if (check < 5)
            effect = zin_eff::smite;
        else if (check < 10)
            effect = zin_eff::silver_corona;
        else if (check < 15)
            effect = zin_eff::ignite_chaos;
        else
            effect = zin_eff::saltify;
        break;

    case RECITE_IMPURE:
        if (check < 5)
            effect = zin_eff::smite;
        else if (check < 10)
            effect = zin_eff::silver_corona;
        else if (check < 15)
        {
            if (mon->undead_or_demonic() && coinflip())
                effect = zin_eff::holy_word;
            else
                effect = zin_eff::silver_corona;
        }
        else
            effect = zin_eff::saltify;
        break;

    case RECITE_UNHOLY:
        if (check < 5)
        {
            if (coinflip())
                effect = zin_eff::daze;
            else
                effect = zin_eff::confuse;
        }
        else if (check < 10)
        {
            if (coinflip())
                effect = zin_eff::confuse;
            else
                effect = zin_eff::silver_corona;
        }
        // Half of the time, the anti-unholy prayer will be capped at this
        // level of effect.
        else if (check < 15 || coinflip())
        {
            if (coinflip())
                effect = zin_eff::holy_word;
            else
                effect = zin_eff::silver_corona;
        }
        else
            effect = zin_eff::saltify;
        break;

    case NUM_RECITE_TYPES:
        die("invalid recite type");
    }

    // And the actual effects...
    switch (effect)
    {
    case zin_eff::nothing:
        break;

    case zin_eff::daze:
        if (mon->add_ench(mon_enchant(ENCH_DAZED, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(*mon, " is dazed by your recitation.");
            affected = true;
        }
        break;

    case zin_eff::confuse:
        if (!mon->clarity()
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

    case zin_eff::paralyse:
        if (mon->add_ench(mon_enchant(ENCH_PARALYSIS, 0, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(*mon,
                minor ? " is awed by your recitation."
                      : " is aghast at the heresy of your recitation.");
            affected = true;
        }
        break;

    case zin_eff::smite:
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

    case zin_eff::blind:
        if (mon->add_ench(mon_enchant(ENCH_BLIND, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(*mon, " is struck blind by the wrath of Zin!");
            affected = true;
        }
        break;

    case zin_eff::silver_corona:
        if (mon->add_ench(mon_enchant(ENCH_SILVER_CORONA, degree, &you,
                          (degree + random2(spellpower)) * BASELINE_DELAY)))
        {
            simple_monster_message(*mon, " is limned with silver light.");
            affected = true;
        }
        break;

    case zin_eff::antimagic:
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

    case zin_eff::mute:
        if (mon->add_ench(mon_enchant(ENCH_MUTE, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(*mon, " is struck mute by the wrath of Zin!");
            affected = true;
        }
        break;

    case zin_eff::mad:
        if (mon->add_ench(mon_enchant(ENCH_MAD, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(*mon, " is driven mad by the wrath of Zin!");
            affected = true;
        }
        break;

    case zin_eff::dumb:
        if (mon->add_ench(mon_enchant(ENCH_DUMB, degree, &you, INFINITE_DURATION)))
        {
            simple_monster_message(*mon, " is left stupefied by the wrath of Zin!");
            affected = true;
        }
        break;

    case zin_eff::ignite_chaos:
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
                    monster_die(*mon, KILL_YOU, NON_MONSTER);
                }
            }
        }
        break;

    case zin_eff::saltify:
        _zin_saltify(mon);
        break;

    case zin_eff::holy_word:
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
    const monster_type pillar_type = mons_species(mons_base_type(*mon));
    const int hd = mon->get_hit_dice();

    simple_monster_message(*mon, " is turned into a pillar of salt by the wrath of Zin!");

    // If the monster leaves a corpse when it dies, destroy the corpse.
    item_def* corpse = monster_die(*mon, KILL_YOU, NON_MONSTER);
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

spret zin_imprison(const coord_def& target, bool fail)
{
    monster* mons = monster_at(target);
    if (mons == nullptr || !you.can_see(*mons))
    {
        mpr("You can see no monster there to imprison!");
        return spret::abort;
    }

    if (mons_is_firewood(*mons) || mons_is_conjured(mons->type))
    {
        mpr("You cannot imprison that!");
        return spret::abort;
    }

    if (mons->friendly() || mons->good_neutral())
    {
        mpr("You cannot imprison a law-abiding creature!");
        return spret::abort;
    }

    int power = 3 + (roll_dice(5, you.skill(SK_INVOCATIONS, 5) + 12) / 26);

    return cast_tomb(power, mons, -GOD_ZIN, fail);
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

    // Duration from 35-80 turns.
    you.set_duration(DUR_DIVINE_SHIELD,
                     35 + you.skill_rdiv(SK_INVOCATIONS, 5, 3));

    // Size of SH bonus.
    you.attribute[ATTR_DIVINE_SHIELD] =
        12 + you.skill_rdiv(SK_INVOCATIONS, 4, 5);

    you.redraw_armour_class = true;
}

void tso_remove_divine_shield()
{
    mprf(MSGCH_DURATION, "Your divine shield fades away.");
    you.duration[DUR_DIVINE_SHIELD] = 0;
    you.attribute[ATTR_DIVINE_SHIELD] = 0;
    you.redraw_armour_class = true;
}

void elyvilon_purification()
{
    mpr("You feel purified!");

    you.duration[DUR_SICKNESS] = 0;
    you.duration[DUR_POISONING] = 0;
    you.duration[DUR_CONF] = 0;
    you.duration[DUR_SLOW] = 0;
    you.duration[DUR_PETRIFYING] = 0;
    you.duration[DUR_WEAK] = 0;
    restore_stat(STAT_ALL, 0, false);
    undrain_hp(9999);
    you.redraw_evasion = true;
}

void elyvilon_divine_vigour()
{
    if (you.duration[DUR_DIVINE_VIGOUR])
        return;

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
    if (spell_typematch(spell, spschool::conjuration))
        return true;

    // Conjurations work by conjuring up a chunk of short-lived matter and
    // propelling it towards the victim. This is the most popular way, but
    // by no means it has a monopoly for being destructive.
    // Vehumet loves all direct physical destruction.
    if (spell == SPELL_SHATTER
        || spell == SPELL_LRD
        || spell == SPELL_SANDBLAST
        || spell == SPELL_AIRSTRIKE
        || spell == SPELL_POLAR_VORTEX
        || spell == SPELL_FREEZE
        || spell == SPELL_IGNITE_POISON
        || spell == SPELL_OZOCUBUS_REFRIGERATION
        || spell == SPELL_OLGREBS_TOXIC_RADIANCE
        || spell == SPELL_VIOLENT_UNRAVELLING
        || spell == SPELL_INNER_FLAME
        || spell == SPELL_BLASTSPARK
        || spell == SPELL_IGNITION
        || spell == SPELL_FROZEN_RAMPARTS
        || spell == SPELL_MAXWELLS_COUPLING
        || spell == SPELL_NOXIOUS_BOG
        || spell == SPELL_POISONOUS_VAPOURS
        || spell == SPELL_SCORCH)
    {
        return true;
    }

    return false;
}

void trog_do_trogs_hand(int pow)
{
    you.increase_duration(DUR_TROGS_HAND,
                          5 + roll_dice(2, pow / 3 + 1), 100,
                          "Your skin crawls.");
    mprf(MSGCH_DURATION, "You feel strong-willed.");
}

void trog_remove_trogs_hand()
{
    mprf(MSGCH_DURATION, "Your skin stops crawling.");
    mprf(MSGCH_DURATION, "You feel less strong-willed.");
    you.duration[DUR_TROGS_HAND] = 0;
}

/**
 * Has the monster been given a Beogh gift?
 *
 * @param mon the orc in question.
 * @returns whether you have given the monster a Beogh gift before now.
 */
bool given_gift(const monster* mon)
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

    if (given_gift(mons))
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
    args.self = confirm_prompt_type::cancel;
    args.show_floor_desc = true;
    args.top_prompt = "Select a follower to give a gift to.";

    direction(spd, args);

    if (!spd.isValid)
    {
        canned_msg(MSG_OK);
        return false;
    }

    monster* mons = monster_at(spd.target);
    if (!beogh_can_gift_items_to(mons, false))
        return false;

    int item_slot = prompt_invent_item("Give which item?",
                                       menu_type::invlist, OSEL_BEOGH_GIFT);

    if (item_slot == PROMPT_ABORT || item_slot == PROMPT_NOTHING)
    {
        canned_msg(MSG_OK);
        return false;
    }

    item_def& gift = you.inv[item_slot];

    const bool shield = is_offhand(gift);
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

    const auto mslot = body_armour ? MSLOT_ARMOUR :
                                    shield ? MSLOT_SHIELD :
                              use_alt_slot ? MSLOT_ALT_WEAPON :
                                             MSLOT_WEAPON;

    item_def *floor_item = mons->take_item(item_slot, mslot);
    if (!floor_item)
    {
        // this probably means move_to_grid in drop_item failed?
        mprf(MSGCH_ERROR, "Gift failed: %s is unable to take %s.",
                                        mons->name(DESC_THE, false).c_str(),
                                        gift.name(DESC_THE, false).c_str());
        return false;
    }
    if (use_alt_slot)
        mons->swap_weapons();

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
                       + si->props[ORC_CORPSE_KEY].get_monster().full_name(DESC_THE)
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
    beogh_convert_orc(mon, conv_t::resurrection);

    return true;
}

bool yred_can_bind_soul(monster* mon)
{
    return mons_can_be_spectralised(*mon, true)
           && !mons_bound_body_and_soul(*mon)
           && mon->attitude != ATT_FRIENDLY;
}

void yred_make_bound_soul(monster* mon, bool force_hostile)
{
    ASSERT(mon); // XXX: change to monster &mon
    ASSERT(mons_bound_body_and_soul(*mon));

    add_daction(DACT_OLD_CHARMD_SOULS_POOF);
    remove_bound_soul_companion();

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
    for (int slot = MSLOT_WEAPON; slot <= MSLOT_ALT_WEAPON; slot++)
    {
        item_def *wpn = mon->mslot_item(static_cast<mon_inv_type>(slot));
        if (wpn && get_weapon_brand(*wpn) == SPWPN_HOLY_WRATH)
        {
            set_item_ego_type(*wpn, OBJ_WEAPONS, SPWPN_DRAINING);
            convert2bad(*wpn);
        }
    }
    monster_drop_things(mon, false, [](const item_def& item)
                                    { return is_holy_item(item); });
    mon->remove_summons();

    const monster orig = *mon;

    // Use the original monster type as the zombified type here, to get
    // the proper stats from it.
    define_zombie(mon, mon->type, MONS_BOUND_SOUL);

    // If the original monster has been levelled up, its HD might be different
    // from its class HD, in which case its HP should be rerolled to match.
    if (mon->get_experience_level() != orig.get_experience_level())
    {
        mon->set_hit_dice(max(orig.get_experience_level(), 1));
        roll_zombie_hp(mon);
    }

    mon->flags |= MF_NO_REWARD;

    // If the original monster type has melee abilities, make sure
    // its spectral thing has them as well.
    mon->flags |= orig.flags & MF_MELEE_MASK;
    monster_spells spl = orig.spells;
    for (const mon_spell_slot &slot : spl)
        if (!(get_spell_flags(slot.spell) & spflag::holy))
            mon->spells.push_back(slot);
    if (mon->spells.size())
        mon->props[CUSTOM_SPELLS_KEY] = true;

    name_zombie(*mon, orig);

    mons_make_god_gift(*mon, GOD_YREDELEMNUL);
    add_companion(mon);

    mon->attitude = !force_hostile ? ATT_FRIENDLY : ATT_HOSTILE;
    behaviour_event(mon, ME_ALERT, force_hostile ? &you : 0);

    mons_att_changed(mon);

    mon->stop_constricting_all();
    mon->stop_being_constricted();

    if (orig.halo_radius()
        || orig.umbra_radius()
        || orig.silence_radius())
    {
        invalidate_agrid();
    }

    // schedule our actual revival for the end of this combat round
    avoided_death_fineff::schedule(mon);

    mprf("%s soul %s.", whose.c_str(),
         !force_hostile ? "is now yours" : "fights you");
}

bool kiku_gift_capstone_spells()
{
    ASSERT(can_do_capstone_ability(you.religion));

    vector<spell_type> spells;
    vector<spell_type> candidates = { SPELL_HAUNT,
                                      SPELL_BORGNJORS_REVIVIFICATION,
                                      SPELL_INFESTATION,
                                      SPELL_NECROMUTATION,
                                      SPELL_DEATHS_DOOR };

    for (auto spell : candidates)
        if (!spell_is_useless(spell, false))
            spells.push_back(spell);

    if (spells.empty())
    {
        simple_god_message(" has no more spells that you can make use of!");
        return false;
    }

    string msg = "Do you wish to receive knowledge of "
                 + comma_separated_fn(spells.begin(), spells.end(), spell_title)
                 + "?";

    if (!yesno(msg.c_str(), true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    simple_god_message(" grants you forbidden knowledge!");
    library_add_spells(spells);
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

static bool _lugonu_warp_monster(monster& mon)
{
    // XXX: should this ignore mon.no_tele(), as with the player?
    if (mon.wont_attack() || mon.no_tele() || coinflip())
        return false;

    mon.blink();
    return true;
}

void lugonu_bend_space()
{
    mpr("Space bends violently around you!");
    uncontrolled_blink(true);
    apply_monsters_around_square(_lugonu_warp_monster, you.pos());
}

void cheibriados_time_bend(int pow)
{
    mpr("The flow of time bends around you.");

    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        monster* mon = monster_at(*ai);
        if (mon && !mon->is_stationary())
        {
            int res_margin = roll_dice(mon->get_hit_dice(), 3);
            res_margin -= random2avg(pow, 2);
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
    if (mon == nullptr || mon->is_stationary() || mon->cannot_act()
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

spret cheibriados_slouch(bool fail)
{
    int count = apply_area_visible(_slouchable, you.pos());
    if (!count)
        if (!yesno("There's no one hasty visible. Invoke Slouch anyway?",
                   true, 'n'))
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }

    targeter_radius hitfunc(&you, LOS_DEFAULT);
    if (stop_attack_prompt(hitfunc, "Slouch", _act_slouchable))
        return spret::abort;

    fail_check();

    mpr("You can feel time thicken for a moment.");
    dprf("your speed is %d", player_movement_speed());

    apply_area_visible(_slouch_monsters, you.pos());
    return spret::success;
}

static void _run_time_step()
{
    ASSERT(you.duration[DUR_TIME_STEP] > 0);
    do
    {
        run_environment_effects();
        handle_monsters();
        manage_clouds();
    }
    while (--you.duration[DUR_TIME_STEP] > 0);
}

// A low-duration step from time, allowing monsters to get closer
// to the player safely.
void cheibriados_temporal_distortion()
{
    you.duration[DUR_TIME_STEP] = 3 + random2(3);

    {
        player_vanishes absent;

        _run_time_step();

        // why only here and not for step from time?
        you.los_noise_level = 0;
        you.los_noise_last_turn = 0;
    }

    you.duration[DUR_TIME_STEP] = 0;

    mpr("You warp the flow of time around you!");
}

void cheibriados_time_step(int pow) // pow is the number of turns to skip
{
    mpr("You step out of the flow of time.");
    flash_view(UA_PLAYER, LIGHTBLUE);
    you.duration[DUR_TIME_STEP] = pow;
    {
        player_vanishes absent(true);

        you.time_taken = 10;
        _run_time_step();
        // Update corpses, etc.
        update_level(pow * 10);

#ifndef USE_TILE_LOCAL
        scaled_delay(1000);
#endif

    }
    you.duration[DUR_TIME_STEP] = 0;

    flash_view(UA_PLAYER, 0);
    mpr("You return to the normal time flow.");
}

struct curse_data
{
    string name;
    string abbr;
    vector<skill_type> boosted;
};

static map<curse_type, curse_data> _ashenzari_curses =
{
    { CURSE_MELEE, {
        "Melee Combat", "Melee",
        { SK_SHORT_BLADES, SK_LONG_BLADES, SK_AXES, SK_MACES_FLAILS,
            SK_POLEARMS, SK_STAVES, SK_UNARMED_COMBAT },
    } },
    { CURSE_RANGED, {
        // XXX: merge with evocations..?
        "Ranged Combat", "Range",
        { SK_RANGED_WEAPONS, SK_THROWING },
    } },
    { CURSE_ELEMENTS, {
        "Elements", "Elem",
        { SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC },
    } },
    { CURSE_ALCHEMY, {
        "Alchemy", "Alch",
        { SK_POISON_MAGIC, SK_TRANSMUTATIONS },
    } },
    { CURSE_COMPANIONS, {
        "Companions", "Comp",
        { SK_SUMMONINGS, SK_NECROMANCY },
    } },
    { CURSE_BEGUILING, {
        "Beguiling", "Bglg",
        { SK_CONJURATIONS, SK_HEXES, SK_TRANSLOCATIONS },
    } },
    { CURSE_SELF, {
        "Introspection", "Self",
        { SK_FIGHTING, SK_SPELLCASTING },
    } },
    { CURSE_FORTITUDE, {
        "Fortitude", "Fort",
        { SK_ARMOUR, SK_SHIELDS },
    } },
    { CURSE_CUNNING, {
        "Cunning", "Cun",
        { SK_DODGING, SK_STEALTH },
    } },
    { CURSE_EVOCATIONS, {
        "Evocations", "Evo",
        { SK_EVOCATIONS },
    } },
};

static bool _can_use_curse(const curse_data& c)
{
    for (skill_type sk : c.boosted)
        if (you.can_currently_train[sk])
            return true;

    return false;
}

string curse_name(const CrawlStoreValue& c)
{
    return _ashenzari_curses[static_cast<curse_type>(c.get_int())].name;
}

string curse_abbr(const CrawlStoreValue& curse)
{
    const curse_data& c =
        _ashenzari_curses[static_cast<curse_type>(curse.get_int())];

    return c.abbr;
}

const vector<skill_type>& curse_skills(const CrawlStoreValue& curse)
{
    const curse_data& c =
        _ashenzari_curses[static_cast<curse_type>(curse.get_int())];

    return c.boosted;
}

static string ashenzari_curse_knowledge_list()
{
    if (!you_worship(GOD_ASHENZARI))
        return "";

    const CrawlVector& curses = you.props[CURSE_KNOWLEDGE_KEY].get_vector();

    return lowercase_string(comma_separated_fn(curses.begin(), curses.end(),
                              curse_name));
}

string desc_curse_skills(const CrawlStoreValue& curse)
{
    const curse_data& c =
        _ashenzari_curses[static_cast<curse_type>(curse.get_int())];

    vector<skill_type> trainable;

    for (skill_type sk : c.boosted)
        if (you.can_currently_train[sk])
            trainable.push_back(sk);

    return c.name + ": "
           + comma_separated_fn(trainable.begin(), trainable.end(), skill_name);
}

/**
 * Choose skills to boost accompanying the current curse.
 */
static void _choose_curse_knowledge()
{
    // This loop choses two available skills without replacement,
    // it is a two element version of a reservoir sampling algorithm.
    //
    // If Ashenzari curses need some fancier weighting this is the
    // place to do that weighting.
    curse_type first_choice = NUM_CURSES;
    curse_type second_choice = NUM_CURSES;
    int valid_curses = 0;
    for (auto const& curse : _ashenzari_curses)
    {
        if (_can_use_curse(curse.second))
        {
            ++valid_curses;
            if (valid_curses == 1)
                first_choice = curse.first;
            else if (valid_curses == 2)
            {
                second_choice = curse.first;
                if (coinflip())
                    swap(first_choice, second_choice);
            }
            else if (one_chance_in(valid_curses))
                first_choice = curse.first;
            else if (one_chance_in(valid_curses - 1))
                second_choice = curse.first;
        }
    }

    you.props.erase(CURSE_KNOWLEDGE_KEY);
    CrawlVector &curses = you.props[CURSE_KNOWLEDGE_KEY].get_vector();

    if (first_choice != NUM_CURSES)
        curses.push_back(first_choice);
    if (second_choice != NUM_CURSES)
        curses.push_back(second_choice);

    // It's not an error for this to be empty, curses are still useful for
    // piety alone
}

/**
 * Offer a new curse to the player, letting them know their new curse is
 * available.
 */
void ashenzari_offer_new_curse()
{
    // No curse at full piety, since shattering resets the curse timer anyway
    if (piety_rank() > 5)
        return;

    _choose_curse_knowledge();

    you.props[AVAILABLE_CURSE_KEY] = true;
    you.props[ASHENZARI_CURSE_PROGRESS_KEY] = 0;
    const string curse_names = ashenzari_curse_knowledge_list();
    const string offer_string = curse_names.empty() ? "" :
                                (" of " + curse_names);

    mprf(MSGCH_GOD, "Ashenzari invites you to partake of a vision"
                    " and a curse%s.", offer_string.c_str());
}

static void _do_curse_item(item_def &item)
{
    mprf("Your %s glows black for a moment.", item.name(DESC_PLAIN).c_str());
    item.flags |= ISFLAG_CURSED;

    if (you.equip[EQ_WEAPON] == item.link)
    {
        // Redraw the weapon.
        you.wield_change = true;
    }

    for (auto & curse : you.props[CURSE_KNOWLEDGE_KEY].get_vector())
    {
        add_inscription(item,
                curse_abbr(static_cast<curse_type>(curse.get_int())));
        item.props[CURSE_KNOWLEDGE_KEY].get_vector().push_back(curse);
    }
}

/**
 * Give a prompt to curse an item.
 *
 * This is the core logic behind Ash's Curse Item ability.
 * Player can abort without penalty.
 * Player can curse only worn items.
 *
 * @return       Whether the player cursed anything.
 */
bool ashenzari_curse_item()
{
    const string prompt_msg = make_stringf("Curse which item? (Esc to abort)");
    const int item_slot = prompt_invent_item(prompt_msg.c_str(),
                                             menu_type::invlist,
                                             OSEL_CURSABLE, OPER_ANY,
                                             invprompt_flag::escape_only);
    if (prompt_failed(item_slot))
        return false;

    item_def& item(you.inv[item_slot]);

    if (!item_is_cursable(item))
    {
        mpr("You can't curse that!");
        return false;
    }

    _do_curse_item(item);
    make_ashenzari_randart(item);
    ash_check_bondage();

    you.props.erase(CURSE_KNOWLEDGE_KEY);
    you.props.erase(AVAILABLE_CURSE_KEY);

    return true;
}

/**
 * Give a prompt to uncurse (and destroy an item).
 *
 * Player can abort without penalty.
 *
 * @return      Whether the player uncursed anything.
 */
bool ashenzari_uncurse_item()
{
    int item_slot = prompt_invent_item("Uncurse and destroy which item?",
                                       menu_type::invlist,
                                       OSEL_CURSED_WORN, OPER_ANY,
                                       invprompt_flag::escape_only);
    if (prompt_failed(item_slot))
        return false;

    item_def& item(you.inv[item_slot]);

    if (is_unrandom_artefact(item, UNRAND_FINGER_AMULET)
        && you.equip[EQ_RING_AMULET] != -1)
    {
        mprf(MSGCH_PROMPT, "You must shatter the curse binding the ring to "
                           "the amulet's finger first!");
        return false;
    }

    if (item_is_melded(item))
    {
        mprf(MSGCH_PROMPT, "You cannot shatter the curse on %s while it is "
                           "melded with your body!",
             item.name(DESC_THE).c_str());
        return false;
    }

    if (!yesno(make_stringf("Really remove and destroy %s?%s",
                            item.name(DESC_THE).c_str(),
                            you.props.exists(AVAILABLE_CURSE_KEY) ?
                                " Ashenzari will withdraw the offered vision "
                                "and curse!"
                                : "").c_str(),
                            false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    mprf("You shatter the curse binding %s!", item.name(DESC_THE).c_str());
    unequip_item(item_equip_slot(you.inv[item_slot]));
    ash_check_bondage();

    you.props[ASHENZARI_CURSE_PROGRESS_KEY] = 0;
    if (you.props.exists(AVAILABLE_CURSE_KEY))
    {
        simple_god_message(" withdraws the vision and curse.");
        you.props.erase(AVAILABLE_CURSE_KEY);
    }

    return true;
}

bool can_convert_to_beogh()
{
    if (silenced(you.pos()))
        return false;

    for (monster* m : monster_near_iterator(you.pos(), LOS_NO_TRANS))
        if (mons_allows_beogh_now(*m))
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

spret dithmenos_shadow_step(bool fail)
{
    // You can shadow-step anywhere within your umbra.
    ASSERT(you.umbra_radius() > -1);
    const int range = you.umbra_radius();

    targeter_shadow_step tgt(&you, you.umbra_radius());
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
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }

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
            canned_msg(MSG_OK);
            return spret::abort;
        }

        if (!zot_trap_prompted)
        {
            trap_def* trap = trap_at(site);
            if (trap && trap->type == TRAP_ZOT)
            {
                if (!check_moveto_trap(site, "shadow step",
                                       &trap_prompted))
                {
                    canned_msg(MSG_OK);
                    return spret::abort;
                }
                zot_trap_prompted = true;
            }
            else if (!trap_prompted
                     && !check_moveto_trap(site, "shadow step",
                                           &trap_prompted))
            {
                canned_msg(MSG_OK);
                return spret::abort;
            }
        }

        if (!exclusion_prompted
            && !check_moveto_exclusion(site, "shadow step",
                                       &exclusion_prompted))
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }

        if (!terrain_prompted
            && !check_moveto_terrain(site, "shadow step", "",
                                     &terrain_prompted))
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
    }

    fail_check();

    const coord_def old_pos = you.pos();
    // XXX: This only ever fails if something's on the landing site;
    // perhaps this should be handled more gracefully.
    if (!you.move_to_pos(tgt.landing_site))
    {
        mpr("Something blocks your shadow step.");
        return spret::success;
    }

    const actor *victim = actor_at(sdirect.target);
    mprf("You step into %s shadow.",
         apostrophise(victim->name(DESC_THE)).c_str());
    // Using 'stepped = true' here because it's Shadow *Step*.
    // This helps to evade splash upon landing on water.
    moveto_location_effects(env.grid(old_pos), true, old_pos);

    return spret::success;
}

static potion_type _gozag_potion_list[][4] =
{
    { POT_HEAL_WOUNDS, NUM_POTIONS, NUM_POTIONS, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_CURING, NUM_POTIONS, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_MAGIC, NUM_POTIONS, NUM_POTIONS },
    { POT_CURING, POT_MAGIC, NUM_POTIONS, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_BERSERK_RAGE, NUM_POTIONS, NUM_POTIONS },
    { POT_HASTE, NUM_POTIONS, NUM_POTIONS, NUM_POTIONS },
    { POT_HASTE, POT_HEAL_WOUNDS, NUM_POTIONS, NUM_POTIONS },
    { POT_HASTE, POT_BRILLIANCE, NUM_POTIONS, NUM_POTIONS },
    { POT_HASTE, POT_RESISTANCE, NUM_POTIONS, NUM_POTIONS },
    { POT_BRILLIANCE, POT_MAGIC, NUM_POTIONS, NUM_POTIONS },
    { POT_INVISIBILITY, POT_MIGHT, NUM_POTIONS, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_CURING, POT_MAGIC, NUM_POTIONS },
    { POT_HEAL_WOUNDS, POT_CURING, POT_BERSERK_RAGE, NUM_POTIONS },
    { POT_MIGHT, POT_BRILLIANCE, NUM_POTIONS, NUM_POTIONS },
    { POT_RESISTANCE, NUM_POTIONS, NUM_POTIONS, NUM_POTIONS },
    { POT_RESISTANCE, POT_MIGHT, NUM_POTIONS, NUM_POTIONS },
    { POT_RESISTANCE, POT_MIGHT, POT_HASTE, NUM_POTIONS },
    { POT_RESISTANCE, POT_INVISIBILITY, NUM_POTIONS, NUM_POTIONS },
    { POT_LIGNIFY, POT_MIGHT, POT_RESISTANCE, NUM_POTIONS },
};

static void _gozag_add_potions(CrawlVector &vec, potion_type *which)
{
    for (; *which != NUM_POTIONS; which++)
    {
        // Check cases where a potion is permanently useless to the player
        // species - temporarily useless potions can still be offered.
        if (*which == POT_BERSERK_RAGE
            && !you.can_go_berserk(true, false, true, nullptr, false))
        {
            continue;
        }
        if (*which == POT_HASTE && you.stasis())
            continue;
        if (*which == POT_MAGIC && you.has_mutation(MUT_HP_CASTING))
            continue;
        if (*which == POT_INVISIBILITY && you.has_mutation(MUT_GLOWING))
            continue;
        if (*which == POT_LIGNIFY && you.undead_state(false) == US_UNDEAD)
            continue;

        // Don't add potions which are already in the list
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

bool gozag_setup_potion_petition(bool quiet)
{
    if (you.gold < GOZAG_POTION_PETITION_AMOUNT)
    {
        if (!quiet)
        {
            mprf("You need at least %d gold to purchase potions right now!",
                 GOZAG_POTION_PETITION_AMOUNT);
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
                const int multiplier = random_range(20, 30); // arbitrary

                string key = make_stringf(GOZAG_POTIONS_KEY, i);
                you.props.erase(key);
                you.props[key].new_vector(SV_INT, SFLAG_CONST_TYPE);
                pots[i] = &you.props[key].get_vector();

                do
                {
                    ADD_POTIONS(*pots[i], _gozag_potion_list);
                    if (coinflip())
                        ADD_POTIONS(*pots[i], _gozag_potion_list);
                }
                while (pots[i]->empty());

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

                if (prices[i] <= GOZAG_POTION_PETITION_AMOUNT)
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
    if (env.grid(you.pos()) != DNGN_FLOOR)
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
    return index >= 0 && index < GOZAG_MAX_SHOPS;
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
    int choice = random2(valid_shops.size());
    type = valid_shops[choice];
    // Don't choose this shop type again for this merchant call.
    valid_shops.erase(valid_shops.begin() + choice);
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
    const string type_name = shop_type_name(type);
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
    for (int i = 0; i < GOZAG_MAX_SHOPS; i++)
        mpr_nojoin(MSGCH_PLAIN, _describe_gozag_shop(i).c_str());

    mprf(MSGCH_PROMPT, "Fund which merchant?");
    const int shop_index = toalower(get_ch()) - 'a';
    if (shop_index < 0 || shop_index > GOZAG_MAX_SHOPS - 1)
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
 * Attempt to call the shop specified at the given index at your position.
 *
 * @param index     The index of the shop (in gozag props)
 */
static void _gozag_place_shop(int index)
{
    ASSERT(env.grid(you.pos()) == DNGN_FLOOR);
    keyed_mapspec kmspec;
    kmspec.set_feat(_gozag_shop_spec(index), false);

    feature_spec feat = kmspec.get_feat();
    if (!feat.shop)
        die("Invalid shop spec?");
    place_spec_shop(you.pos(), *feat.shop, you.experience_level);

    link_items();
    env.markers.add(new map_feature_marker(you.pos(), DNGN_ABANDONED_SHOP));
    env.markers.clear_need_activate();

    shop_struct *shop = shop_at(you.pos());
    ASSERT(shop);

    const gender_type gender = random_choose(GENDER_FEMALE, GENDER_MALE,
                                             GENDER_NEUTRAL);

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
#if TAG_MAJOR_VERSION == 34
        if (type == SHOP_FOOD || type == SHOP_EVOKABLES)
            continue;
#endif
        if (type == SHOP_DISTILLERY && you.has_mutation(MUT_NO_DRINK))
            continue;

        if (you.has_mutation(MUT_NO_ARMOUR) &&
            (type == SHOP_ARMOUR
             || type == SHOP_ARMOUR_ANTIQUE))
        {
            continue;
        }
        if ((type == SHOP_WEAPON || type == SHOP_WEAPON_ANTIQUE)
            && you.has_mutation(MUT_NO_GRASPING))
        {
            continue;
        }
        valid_shops.push_back(type);
    }

    // Set up some dummy shops.
    // Generate some shop inventory and store it as a store spec.
    // We still set up the shops in advance in case of hups.
    for (int i = 0; i < GOZAG_MAX_SHOPS; i++)
        if (!you.props.exists(make_stringf(GOZAG_SHOPKEEPER_NAME_KEY, i)))
            _setup_gozag_shop(i, valid_shops);

    const int shop_index = _gozag_choose_shop();
    if (shop_index == -1) // hup!
        return false;

    ASSERT(shop_index >= 0 && shop_index < GOZAG_MAX_SHOPS);

    const int cost = _gozag_shop_price(shop_index);
    ASSERT(you.gold >= cost);

    you.del_gold(cost);
    you.attribute[ATTR_GOZAG_GOLD_USED] += cost;

    _gozag_place_shop(shop_index);

    you.attribute[ATTR_GOZAG_SHOPS]++;
    you.attribute[ATTR_GOZAG_SHOPS_CURRENT]++;

    for (int j = 0; j < GOZAG_MAX_SHOPS; j++)
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
    if (feat_is_branch_entrance(env.grid(you.pos())))
    {
        for (branch_iterator it; it; ++it)
            if (it->entry_stairs == env.grid(you.pos())
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
    if (feat_is_branch_entrance(env.grid(you.pos())))
    {
        for (branch_iterator it; it; ++it)
            if (it->entry_stairs == env.grid(you.pos())
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

spret qazlal_upheaval(coord_def target, bool quiet, bool fail, dist *player_target)
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

    if (target.origin())
    {
        dist target_local;
        if (!player_target)
            player_target = &target_local;

        targeter_smite tgt(&you, LOS_RADIUS, 0, max_radius);
        direction_chooser_args args;
        args.restricts = DIR_TARGET;
        args.mode = TARG_HOSTILE;
        args.needs_path = false;
        args.top_prompt = "Aiming: <white>Upheaval</white>";
        args.self = confirm_prompt_type::cancel;
        args.hitfunc = &tgt;
        if (!spell_direction(*player_target, beam, &args))
            return spret::abort;

        if (cell_is_solid(beam.target))
        {
            mprf("There is %s there.",
                 article_a(feat_type_name(env.grid(beam.target))).c_str());
            return spret::abort;
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
            return spret::abort;
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
           affected.push_back(*ri);
    }
    if (!quiet)
        shuffle_array(affected);

    // for `quiet` calls (i.e. disaster area), don't delay for individual tiles
    // at all -- do the delay per upheaval draw. This will also fully suppress
    // the redraw per tile.
    beam.draw_delay = quiet ? 0 : 25;
    for (coord_def pos : affected)
        beam.draw(pos, false);

    if (quiet)
    {
        // When `quiet`, refresh the view after each complete draw pass.
        // why this call dance to refresh? I just copied it from bolt::draw
        viewwindow(false);
        update_screen();
        scaled_delay(50); // add some delay per upheaval draw, otherwise it all
                          // goes by too fast.
    }
    else
    {
        scaled_delay(200); // This is here to make it easy for the player to
                           // see the overall impact of the upheaval
        mprf(MSGCH_GOD, "%s", message.c_str());
    }

    beam.animate = false; // already drawn

    for (coord_def pos : affected)
    {
        beam.source = pos;
        beam.target = pos;
        beam.fire();

        switch (beam.flavour)
        {
            case BEAM_LAVA:
                if (env.grid(pos) == DNGN_FLOOR && !actor_at(pos) && coinflip())
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
            default:
                break;
        }
    }

    return spret::success;
}

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

vector<coord_def> find_elemental_targets()
{
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

    return targets;
}

spret qazlal_elemental_force(bool fail)
{
    vector<coord_def> targets = find_elemental_targets();
    if (targets.empty())
    {
        mpr("You can't see any clouds you can empower.");
        return spret::abort;
    }

    fail_check();

    shuffle_array(targets);
    const int count = max(1, min((int)targets.size(),
                                 random2avg(you.skill(SK_INVOCATIONS), 2)));
    mgen_data mg;
    mg.summon_type = MON_SUMM_AID;
    mg.abjuration_duration = 1;
    mg.flags |= MG_FORCE_PLACE | MG_AUTOFOE;
    mg.summoner = &you;
    int placed = 0;
    for (unsigned int i = 0; placed < count && i < targets.size(); i++)
    {
        coord_def pos = targets[i];
        ASSERT(cloud_at(pos));
        const cloud_struct &cl = *cloud_at(pos);
        mg.behaviour = BEH_FRIENDLY;
        mg.pos       = pos;
        auto mons_type = map_find(elemental_clouds, cl.type);
        // it is not impossible that earlier placements caused new clouds not
        // in the map.
        if (!mons_type)
            continue;
        mg.cls = *mons_type;
        if (!create_monster(mg))
            continue;
        delete_cloud(pos);
        placed++;
    }

    if (placed)
        mprf(MSGCH_GOD, "Clouds arounds you coalesce and take form!");
    else
        canned_msg(MSG_NOTHING_HAPPENS); // can this ever happen?

    return spret::success;
}

spret qazlal_disaster_area(bool fail)
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
        return spret::abort;
    }

    if (friendlies
        && !yesno("There are friendlies around; are you sure you want to hurt "
                  "them?", true, 'n'))
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }

    fail_check();

    mprf(MSGCH_GOD, "Nature churns violently around you!");

    // TODO: should count get a cap proportional to targets.size()?
    int count = max(1, min((int)targets.size(),
                            max(you.skill_rdiv(SK_INVOCATIONS, 1, 2),
                                random2avg(you.skill(SK_INVOCATIONS, 2), 2))));

    for (int i = 0; i < count; i++)
    {
        if (targets.size() == 0)
            break;
        int which = choose_random_weighted(weights.begin(), weights.end());
        // Downweight adjacent potential targets (but don't rule them out
        // entirely).
        for (unsigned int j = 0; j < targets.size(); j++)
            if (adjacent(targets[which], targets[j]))
                weights[j] = max(weights[j] / 2, 1);
        qazlal_upheaval(targets[which], true);
        targets.erase(targets.begin() + which);
        weights.erase(weights.begin() + which);
    }
    scaled_delay(200);

    return spret::success;
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

/// A map between sacrifice_def.sacrifice_vector strings & possible mut lists.
/// Abilities that map to a single mutation are not here!
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
        MUT_WEAK_WILLED,
        MUT_WEAK_WILLED,
        MUT_LOW_MAGIC,
    }},
    /// Mutations granted by ABIL_RU_SACRIFICE_PURITY
    { PURITY_SAC_KEY, {
        MUT_SCREAM,
        MUT_INHIBITED_REGENERATION,
        MUT_NO_POTION_HEAL,
        MUT_DOPEY,
        MUT_CLUMSY,
        MUT_WEAK,
    }},
};

/// School-disabling mutations that will be painful for most characters.
static const vector<mutation_type> _major_arcane_sacrifices =
{
    MUT_NO_CONJURATION_MAGIC,
    MUT_NO_NECROMANCY_MAGIC,
    MUT_NO_SUMMONING_MAGIC,
    MUT_NO_TRANSLOCATION_MAGIC,
};

/// School-disabling mutations that are unfortunate for most characters.
static const vector<mutation_type> _moderate_arcane_sacrifices =
{
    MUT_NO_TRANSMUTATION_MAGIC,
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

// this function is for checks that can be done with the mutation_type alone.
static bool _sac_mut_maybe_valid(mutation_type mut)
{
    // can't give the player this if they're already at max
    if (you.get_mutation_level(mut) >= mutation_max_levels(mut))
        return false;

    // can't give the player this if they have an innate mut that conflicts
    if (mut_check_conflict(mut, true))
        return false;

    // Don't offer sacrifices of skills that a player already can't use.
    if (!can_sacrifice_skill(mut))
        return false;

    // Special case a few weird interactions:

    // Don't offer to sacrifice summoning magic when already hated by all.
    if (mut == MUT_NO_SUMMONING_MAGIC
        && you.get_mutation_level(MUT_NO_LOVE))
    {
        return false;
    }

    // Vampires can't get inhibited regeneration for some reason related
    // to their existing regen silliness.
    if (mut == MUT_INHIBITED_REGENERATION && you.has_mutation(MUT_VAMPIRISM))
        return false;

    // demonspawn can't get frail if they have a robust facet
    if (you.species == SP_DEMONSPAWN && mut == MUT_FRAIL
        && any_of(begin(you.demonic_traits), end(you.demonic_traits),
                  [] (player::demon_trait t)
                  { return t.mutation == MUT_ROBUST; }))
    {
        return false;
    }

    // No potion heal doesn't affect mummies since they can't quaff potions
    if (mut == MUT_NO_POTION_HEAL && you.has_mutation(MUT_NO_DRINK))
        return false;

    return true;
}

/**
 * Choose a random mutation from the given list, only including those that are
 * valid choices for a Ru sacrifice. (Not already at the max level, not
 * conflicting with an innate mut.)
 * N.b. this is *only* used for choosing among the sublists for sac health,
 * essence, and purity.
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
        if (!_sac_mut_maybe_valid(mut))
            continue;
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

    for (const vector<mutation_type> &arcane_sacrifice_list :
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
    for (const vector<mutation_type> &arcane_sacrifice_list :
                                    _arcane_sacrifice_lists)
    {
        for (mutation_type sacrifice : arcane_sacrifice_list)
            if (you.get_mutation_level(sacrifice))
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
    // for sacrifices other than health, essence, and arcana there is a
    // deterministic mapping between the sacrifice_def and a mutation_type.
    if (sacrifice.mutation != MUT_NON_MUTATION
        && !_sac_mut_maybe_valid(sacrifice.mutation))
    {
        return false;
    }

    // For health, essence, and arcana, we still need to choose from the
    // sublists in sacrifice_vector_map.
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

    // finally, sacrifices may have custom validity checks.
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
    const spschool school = skill2spell_type(sk);
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
    return skill_abbr(arcane_mutation_to_skill(mutation));
}

static int _piety_for_skill(skill_type skill)
{
    // Gnolls didn't have a choice about training the skill, so don't give
    // them more piety for waiting longer before taking the sacrifice.
    if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        return 0;

    // This should be mostly redundant with other checks, but it's a useful
    // sanitizer
    if (is_useless_skill(skill))
        return 0;

    return skill_exp_needed(you.skills[skill], skill, you.species) / 500;
}

static int _piety_for_skill_by_sacrifice(ability_type sacrifice)
{
    int piety_gain = 0;
    const sacrifice_def &sac_def = _get_sacrifice_def(sacrifice);

    piety_gain += _piety_for_skill(sac_def.sacrifice_skill);
    if (sacrifice == ABIL_RU_SACRIFICE_HAND
        && species::size(you.species, PSIZE_TORSO) <= SIZE_SMALL)
    {
        // No one-handed staves for small races.
        piety_gain += _piety_for_skill(SK_STAVES);
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
        case ABIL_RU_SACRIFICE_HEALTH:
            if (mut == MUT_FRAIL)
                piety_gain += 10; // -health is pretty much always quite bad.
            else if (mut == MUT_PHYSICAL_VULNERABILITY)
                piety_gain += 5; // -AC is a bit worse than -EV
            break;
        case ABIL_RU_SACRIFICE_ESSENCE:
            if (mut == MUT_LOW_MAGIC)
            {
                piety_gain += 10 + max(you.skill_rdiv(SK_INVOCATIONS, 1, 2),
                                       you.skill_rdiv(SK_SPELLCASTING, 1, 2));
            }
            else if (mut == MUT_WEAK_WILLED)
                piety_gain += 35;
            else
                piety_gain += 2 + _get_stat_piety(STAT_INT, 6)
                                + you.skill_rdiv(SK_SPELLCASTING, 1, 2);
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
            else if (you.get_mutation_level(mut) == 2)
                piety_gain += 28;
            else if (you.get_mutation_level(mut) == 1)
                piety_gain += 21;
            else
                piety_gain += 14;

            if (mut == MUT_SCREAM)
                piety_gain /= 2; // screaming just isn't that bad.

            break;
        case ABIL_RU_SACRIFICE_ARTIFICE:
            if (you.get_mutation_level(MUT_NO_LOVE))
                piety_gain -= 10; // You've already lost some value here
            break;
        case ABIL_RU_SACRIFICE_SKILL:
            // give a small bonus if sacrifice skill is taken multiple times
            piety_gain += 7 * you.get_mutation_level(mut);
            break;
        case ABIL_RU_SACRIFICE_NIMBLENESS:
            if (you.get_mutation_level(MUT_NO_ARMOUR_SKILL))
                piety_gain += 20;
            else if (species_apt(SK_ARMOUR) == UNUSABLE_SKILL)
                piety_gain += 28; // this sacrifice is worse for these races
            break;
        // words and drink cut off a lot of options if taken together
        case ABIL_RU_SACRIFICE_DRINK:
            // less value if you already have some levels of the mutation
            piety_gain -= 10 * you.get_mutation_level(MUT_DRINK_SAFETY);
            // check innate mutation level to see if reading was sacrificed
            if (you.get_innate_mutation_level(MUT_READ_SAFETY) == 2)
                piety_gain += 10;
            break;
        case ABIL_RU_SACRIFICE_WORDS:
            // less value if you already have some levels of the mutation
            piety_gain -= 10 * you.get_mutation_level(MUT_READ_SAFETY);
            if (you.get_innate_mutation_level(MUT_DRINK_SAFETY) == 2)
                piety_gain += 10;
            else if (you.get_mutation_level(MUT_NO_DRINK))
                piety_gain += 15; // extra bad for mummies
            break;
        case ABIL_RU_SACRIFICE_DURABILITY:
            if (you.get_mutation_level(MUT_NO_DODGING))
                piety_gain += 20;
            break;
        case ABIL_RU_SACRIFICE_LOVE:
            if (you.get_mutation_level(MUT_NO_SUMMONING_MAGIC)
                && you.get_mutation_level(MUT_NO_ARTIFICE))
            {
                // this is virtually useless, aside from zot_tub
                piety_gain = 1;
            }
            else if (you.get_mutation_level(MUT_NO_SUMMONING_MAGIC)
                     || you.get_mutation_level(MUT_NO_ARTIFICE))
            {
                piety_gain /= 2;
            }
            break;
        case ABIL_RU_SACRIFICE_EXPERIENCE:
            if (you.get_mutation_level(MUT_COWARDICE))
                piety_gain += 6;
            // Ds are highly likely to miss at least one mutation. This isn't
            // absolutely certain, but it's very likely and they should still
            // get a bonus for the risk. Could check the exact mutation
            // schedule, but this seems too leaky.
            // Dj are guaranteed to lose a spell for the first and third sac,
            // which is pretty sad too.
            if (you.species == SP_DEMONSPAWN
                || you.species == SP_DJINNI && (you.get_mutation_level(MUT_INEXPERIENCED) % 2 == 0))
            {
                piety_gain += 10;
            }
            break;
        case ABIL_RU_SACRIFICE_COURAGE:
            piety_gain += 6 * you.get_mutation_level(MUT_INEXPERIENCED);
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
        return "a medium";
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
    if (sacrifice == MUT_READ_SAFETY || sacrifice == MUT_DRINK_SAFETY)
    {
        // get the safety mutation to the cap instead of 1 level higher
        perma_mutate(sacrifice,
                    3 - you.get_mutation_level(sacrifice),
                    "Ru sacrifice");
    }
    else
    {
        // regular case for other sacrifices
        perma_mutate(sacrifice, 1, "Ru sacrifice");
    }
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
    you.can_currently_train.set(skill, false);
    reset_training();
    check_selected_skills();
}

static void _extra_sacrifice_code(ability_type sac)
{
    const sacrifice_def &sac_def = _get_sacrifice_def(sac);
    if (sac_def.sacrifice == ABIL_RU_SACRIFICE_HAND)
    {
        auto ring_slots = species::ring_slots(you.species, true);
        equipment_type sac_ring_slot = species::sacrificial_arm(you.species);

        item_def* const shield = you.slot_item(EQ_SHIELD, true);
        item_def* const weapon = you.slot_item(EQ_WEAPON, true);
        item_def* const ring = you.slot_item(sac_ring_slot, true);
        int ring_inv_slot = you.equip[sac_ring_slot];
        equipment_type open_ring_slot = EQ_NONE;

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
            // XX does not handle an open slot on the finger amulet
            for (const auto &eq : ring_slots)
                if (!you.slot_item(eq, true))
                {
                    open_ring_slot = eq;
                    break;
                }

            const bool can_keep = open_ring_slot != EQ_NONE;

            mprf("You can no longer wear %s!",
                ring->name(DESC_YOUR).c_str());
            unequip_item(sac_ring_slot, true, can_keep);
            if (can_keep)
            {
                mprf("You put %s back on %s %s!",
                     ring->name(DESC_YOUR).c_str(),
                     (ring_slots.size() > 1 ? "another" : "your other"),
                     you.hand_name(true).c_str());
                equip_item(open_ring_slot, ring_inv_slot, false, true);
            }
        }
    }
    else if (sac_def.sacrifice == ABIL_RU_SACRIFICE_EXPERIENCE)
        level_change();
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
        update_screen();
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
    if (!you_worship(GOD_RU))
        return "";

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
    // save piety gain, since sacrificing skills can lower the piety gain
    const int piety_gain = _ru_get_sac_piety_gain(sac);
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
    if (sac == ABIL_RU_SACRIFICE_HAND
        && species::size(you.species, PSIZE_TORSO) <= SIZE_SMALL)
    {
        // No one-handed staves for small races.
        _ru_kill_skill(SK_STAVES);
    }

    mark_milestone("sacrifice", mile_text);

    // Any special handling that's needed.
    _extra_sacrifice_code(sac);

    // Update how many Ru sacrifices you have. This is used to avoid giving the
    // player extra silver damage.
    if (you.props.exists(NUM_SACRIFICES_KEY))
    {
        you.props[NUM_SACRIFICES_KEY] = num_sacrifices +
            you.props[NUM_SACRIFICES_KEY].get_int();
    }
    else
        you.props[NUM_SACRIFICES_KEY] = num_sacrifices;

    // Actually give the piety for this sacrifice.
    set_piety(min(piety_breakpoint(5), you.piety + piety_gain));

    if (you.piety == piety_breakpoint(5))
        simple_god_message(" indicates that your awakening is complete.");

    // Clean up.
    _ru_expire_sacrifices();
    ru_reset_sacrifice_timer(true);
    redraw_screen(); // pretty much everything could have changed
    update_screen();
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
    const int base_delay = 90;
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

    delay = div_rand_round((delay + added_delay) * (3 - you.faith()), 3);
    if (crawl_state.game_is_sprint())
        delay /= SPRINT_MULTIPLIER;

    you.props[RU_SACRIFICE_DELAY_KEY] = delay;
    you.props[RU_SACRIFICE_PROGRESS_KEY] = 0;
}

// Check to see if you're eligible to retaliate.
//Your chance of eligiblity scales with piety.
bool will_ru_retaliate()
{
    // Scales up to a 20% chance of retribution
    return have_passive(passive_t::upgraded_aura_of_power)
           && crawl_state.which_god_acting() != GOD_RU
           && one_chance_in(div_rand_round(800, you.piety));
}

// Power of retribution increases with damage, decreases with monster HD.
void ru_do_retribution(monster* mons, int damage)
{
    int power = max(0, random2(div_rand_round(you.piety*10, 32))
        + damage - (2 * mons->get_hit_dice()));
    const actor* act = &you;

    if (power > 50 && (mons->antimagic_susceptible()))
    {
        mprf(MSGCH_GOD, "You focus your inner power and drain %s's magic in "
                "retribution!", mons->name(DESC_THE).c_str());
        mons->add_ench(mon_enchant(ENCH_ANTIMAGIC, 1, act, power+random2(320)));
    }
    else if (power > 35)
    {
        mprf(MSGCH_GOD, "You focus your inner power and paralyse %s in retribution!",
                mons->name(DESC_THE).c_str());
        mons->add_ench(mon_enchant(ENCH_PARALYSIS, 1, act, power+random2(60)));
    }
    else if (power > 25)
    {
        mprf(MSGCH_GOD, "You focus your inner power and slow %s in retribution!",
                mons->name(DESC_THE).c_str());
        mons->add_ench(mon_enchant(ENCH_SLOW, 1, act, power+random2(100)));
    }
    else if (power > 10 && mons_can_be_blinded(mons->type))
    {
        mprf(MSGCH_GOD, "You focus your inner power and blind %s in retribution!",
                mons->name(DESC_THE).c_str());
        mons->add_ench(mon_enchant(ENCH_BLIND, 1, act, power+random2(100)));
    }
    else if (power > 0)
    {
        mprf(MSGCH_GOD, "You focus your inner power and illuminate %s in retribution!",
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
            // XXX: destroying them is dubious in general - abuseable by loons?
            // (but definitely destroy if ammo == 1, per trap-def.h!)
            mpr("You burst free from the webs!");
        }
    }
    else
    {
        destroy_item(net);
        mpr("You burst free from the net!");
    }
    stop_being_held();

    // Escape constriction
    you.stop_being_constricted(false);

    // cancel petrification/confusion/slow
    you.duration[DUR_CONF] = 0;
    you.duration[DUR_SLOW] = 0;
    you.duration[DUR_PETRIFYING] = 0;

    int hp_inc = div_rand_round(you.piety, 16);
    hp_inc += roll_dice(div_rand_round(you.piety, 20), 6);
    inc_hp(hp_inc);
    int mp_inc = div_rand_round(you.piety, 48);
    mp_inc += roll_dice(div_rand_round(you.piety, 40), 4);
    inc_mp(mp_inc);
    drain_player(30, false, true);
}

bool ru_power_leap()
{
    ASSERT(!crawl_state.game_is_arena());

    if (crawl_state.is_replaying_keys())
    {
        crawl_state.cancel_cmd_all("You can't repeat Power Leap.");
        return false;
    }
    if (you.is_nervous())
    {
        mpr("You are too terrified to leap around!");
        return false;
    }

    // query for location:
    dist beam;

    while (1)
    {
        direction_chooser_args args;
        args.restricts = DIR_ENFORCE_RANGE;
        args.mode = TARG_ANY;
        args.range = 3;
        args.needs_path = false;
        args.top_prompt = "Aiming: <white>Power Leap</white>";
        args.self = confirm_prompt_type::cancel;
        const int explosion_size = 1;
        targeter_smite tgt(&you, args.range, explosion_size, explosion_size);
        tgt.obeys_mesmerise = true;
        args.hitfunc = &tgt;
        direction(beam, args);
        if (crawl_state.seen_hups)
        {
            clear_messages();
            mpr("Cancelling leap due to HUP.");
            return false;
        }

        if (!beam.isValid || beam.target == you.pos())
        {
            canned_msg(MSG_OK);
            return false;         // early return
        }

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

        if (env.grid(beam.target) == DNGN_OPEN_SEA)
        {
            clear_messages();
            mpr("You can't leap into the sea!");
            continue;
        }
        else if (env.grid(beam.target) == DNGN_LAVA_SEA)
        {
            clear_messages();
            mpr("You can't leap into the sea of lava!");
            continue;
        }
        else if (!check_moveto(beam.target, "leap", false))
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

    if (!you.attempt_escape(2)) // returns true if not constricted
        return true;

    if (cell_is_solid(beam.target) || monster_at(beam.target))
    {
        // XXX: try to jump somewhere nearby?
        mpr("Something unexpectedly blocked you, preventing you from leaping!");
        return true;
    }

    move_player_to_grid(beam.target, false);
    apply_barbs_damage();

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
                message = " doubts " + mons->pronoun(PRONOUN_POSSESSIVE)
                          + " magic when faced with ultimate truth!";
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
            canned_msg(MSG_OK);
            return false;
        }
    }
    mpr("You reveal the great annihilating truth to your foes!");
    noisy(30, you.pos());
    apply_area_visible(_apply_apocalypse, you.pos());
    drain_player(100, false, true);
    return true;
}

static bool _mons_stompable(const monster &mons)
{
    // Don't hurt your own demonic guardians
    return !testbits(mons.flags, MF_DEMONIC_GUARDIAN) || !mons.friendly();
}

static bool _get_stomped(monster& mons)
{
    if (!_mons_stompable(mons))
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
    // Demonic guardians are immune but check for other friendlies
    const bool friendlies = apply_monsters_around_square([] (monster& mons) {
        return _mons_stompable(mons) && mons_att_wont_attack(mons.attitude);
    }, you.pos());

    // XXX: this 'friendlies' wording feels a little odd, but we do use it in a
    // a few places already; see spl-vortex.cc, disaster area, etc.
    if (friendlies
        && !yesno("There are friendlies around, "
                  "are you sure you want to hurt them?",
                  true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    mpr("You stomp with the beat, sending a shockwave through the revellers "
            "around you!");
    apply_monsters_around_square(_get_stomped, you.pos());
    return true;
}

bool uskayaw_line_pass()
{
    ASSERT(!crawl_state.game_is_arena());

    if (crawl_state.is_replaying_keys())
    {
        crawl_state.cancel_cmd_all("You can't repeat Line Pass.");
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
        unique_ptr<targeter> hitfunc;
        hitfunc = make_unique<targeter_monster_sequence>(&you, pow, range);

        direction_chooser_args args;
        args.hitfunc = hitfunc.get();
        args.restricts = DIR_ENFORCE_RANGE;
        args.mode = TARG_ANY;
        args.needs_path = false;
        args.top_prompt = "Aiming: <white>Line Pass</white>";
        args.range = 8;

        if (!spell_direction(beam, line_pass, &args))
            return false;

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

        if (env.grid(beam.target) == DNGN_OPEN_SEA)
        {
            clear_messages();
            mpr("You can't line pass into the sea!");
            continue;
        }
        else if (env.grid(beam.target) == DNGN_LAVA_SEA)
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
        else if (!check_moveto(beam.target, "line pass", false))
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
        apply_barbs_damage();
    }

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    return true;
}

spret uskayaw_grand_finale(bool fail)
{
    ASSERT(!crawl_state.game_is_arena());

    if (crawl_state.is_replaying_keys())
    {
        crawl_state.cancel_cmd_all("No encores!");
        return spret::abort;
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
        args.self = confirm_prompt_type::cancel;
        targeter_smite tgt(&you);
        args.hitfunc = &tgt;
        direction(beam, args);
        if (crawl_state.seen_hups)
        {
            clear_messages();
            mpr("Cancelling grand finale due to HUP.");
            return spret::abort;
        }

        if (!beam.isValid || beam.target == you.pos())
        {
            canned_msg(MSG_OK);
            return spret::abort;   // early return
        }

        mons = monster_at(beam.target);
        if (!mons || !you.can_see(*mons))
        {
            clear_messages();
            mpr("You can't perceive a target there!");
            continue;
        }

        if (!check_moveto(beam.target, "move", false))
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

    fail_check();

    ASSERT(mons);

    // kill the target
    if (mons->type == MONS_ROYAL_JELLY && !mons->is_summoned())
    {
        // need to do this here, because react_to_damage is never called
        mprf("%s explodes violently into a cloud of jellies!",
                                        mons->name(DESC_THE, false).c_str());
        trj_spawn_fineff::schedule(&you, mons, mons->pos(), mons->hit_points);
    }
    else
        mprf("%s explodes violently!", mons->name(DESC_THE, false).c_str());
    mons->flags |= MF_EXPLODE_KILL;
    if (!mons->is_insubstantial())
    {
        blood_spray(mons->pos(), mons->mons_species(), mons->hit_points / 5);
        throw_monster_bits(*mons); // have some fun while we're at it
    }

    // throw_monster_bits can cause mons to be killed already, e.g. via pain
    // bond or dismissing summons
    if (mons->alive())
        monster_die(*mons, KILL_YOU, NON_MONSTER, false);

    if (!mons->alive())
        move_player_to_grid(beam.target, false);
    else
        mpr("You spring back to your original position.");

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    set_piety(piety_breakpoint(0)); // Reset piety to 1*.

    return spret::success;
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
spret hepliaklqana_idealise(bool fail)
{
    const mid_t ancestor_mid = hepliaklqana_ancestor();
    if (ancestor_mid == MID_NOBODY)
    {
        mpr("You have no ancestor to preserve!");
        return spret::abort;
    }

    monster *ancestor = monster_by_mid(ancestor_mid);
    if (!ancestor || !you.can_see(*ancestor))
    {
        mprf("%s is not nearby!", hepliaklqana_ally_name().c_str());
        return spret::abort;
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
    return spret::success;
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
    targeter_transference tgt(&you, aoe_radius);
    direction_chooser_args args;
    args.hitfunc = &tgt;
    args.restricts = DIR_TARGET;
    args.mode = TARG_MOBILE_MONSTER;
    args.range = LOS_RADIUS;
    args.needs_path = false;
    args.self = confirm_prompt_type::none;
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
        if (!mon || god_protects(mon))
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
spret hepliaklqana_transference(bool fail)
{
    monster *ancestor = hepliaklqana_ancestor_mon();
    if (!ancestor || !you.can_see(*ancestor))
    {
        mprf("%s is not nearby!", hepliaklqana_ally_name().c_str());
        return spret::abort;
    }

    coord_def target = _get_transference_target();
    if (target.origin())
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }

    actor* victim = actor_at(target);
    const bool victim_visible = victim && you.can_see(*victim);
    if ((!victim || !victim_visible)
        && !yesno("You can't see anything there. Try transferring anyway?",
                  true, 'n'))
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }

    if (victim == ancestor)
    {
        mprf("You can't transfer your ancestor with %s.",
             ancestor->pronoun(PRONOUN_REFLEXIVE).c_str());
        return spret::abort;
    }

    const bool victim_immovable
        = victim && (mons_is_tentacle_or_tentacle_segment(victim->type)
                     || victim->is_stationary());
    if (victim_visible && victim_immovable)
    {
        mpr("You can't transfer that.");
        return spret::abort;
    }

    const coord_def destination = ancestor->pos();
    if (victim == &you && !check_moveto(destination, "transfer", false))
        return spret::abort;

    const bool uninhabitable = victim && !victim->is_habitable(destination);
    if (uninhabitable && victim_visible)
    {
        mprf("%s can't be transferred there.", victim->name(DESC_THE).c_str());
        return spret::abort;
    }

    // we assume the ancestor flies & so can survive anywhere anything can.

    fail_check();

    if (!victim || uninhabitable || victim_immovable)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    if (victim->is_player())
    {
        if (cancel_harmful_move(false))
            return spret::abort;
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

    return spret::success;
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
    static const map<gender_type, string> gender_map =
    {
        { GENDER_NEUTRAL, "neither" },
        { GENDER_MALE,    "male"    },
        { GENDER_FEMALE,  "female"  },
    };

    const gender_type current_gender =
        (gender_type)you.props[HEPLIAKLQANA_ALLY_GENDER_KEY].get_int();
    const string* desc = map_find(gender_map, current_gender);
    ASSERT(desc);

    mprf(MSGCH_PROMPT,
         "Was %s a) male, b) female, or c) neither? (Currently %s.)",
         hepliaklqana_ally_name().c_str(),
         desc->c_str());

    int keyin = toalower(get_ch());
    if (!isaalpha(keyin))
    {
        canned_msg(MSG_OK);
        return;
    }

    static const gender_type gender_options[] = { GENDER_MALE,
                                                  GENDER_FEMALE,
                                                  GENDER_NEUTRAL };

    const uint32_t choice = keyin - 'a';
    if (choice >= ARRAYSZ(gender_options))
    {
        canned_msg(MSG_OK);
        return;
    }

    const gender_type new_gender = gender_options[choice];

    if (new_gender == current_gender)
    {
        canned_msg(MSG_OK);
        return;
    }

    you.props[HEPLIAKLQANA_ALLY_GENDER_KEY] = new_gender;
    mprf("%s was always %s, you're pretty sure.",
         hepliaklqana_ally_name().c_str(),
         map_find(gender_map, new_gender)->c_str());
    upgrade_hepliaklqana_ancestor(true);
}

/// Rename and/or re-gender your ancestor.
void hepliaklqana_choose_identity()
{
    _hepliaklqana_choose_name();
    _hepliaklqana_choose_gender();
}

bool wu_jian_can_wall_jump_in_principle(const coord_def& target)
{
    if (!have_passive(passive_t::wu_jian_wall_jump)
        || !feat_can_wall_jump_against(env.grid(target))
        || !you.is_motile()
        || you.digging)
    {
        return false;
    }
    return true;
}

bool wu_jian_can_wall_jump(const coord_def& target, string &error_ret)
{
    if (target.distance_from(you.pos()) != 1)
    {
        error_ret = "Please select an adjacent position to wall jump against.";
        return false;
    }

    if (!wu_jian_can_wall_jump_in_principle(target))
    {
        if (!feat_can_wall_jump_against(env.grid(target)))
        {
            error_ret = string("You cannot wall jump against ") +
                feature_description_at(target, false, DESC_THE) + ".";
        }
        else
            error_ret = "";
        return false;
    }

    auto wall_jump_direction = (you.pos() - target).sgn();
    auto wall_jump_landing_spot = (you.pos() + wall_jump_direction
                                   + wall_jump_direction);

    monster* beholder = you.get_beholder(wall_jump_landing_spot);
    if (beholder)
    {
        error_ret = make_stringf("You cannot wall jump away from %s!",
             beholder->name(DESC_THE, true).c_str());
        return false;
    }

    monster* fearmonger = you.get_fearmonger(wall_jump_landing_spot);
    if (fearmonger)
    {
        error_ret = make_stringf("You cannot wall jump closer to %s!",
             fearmonger->name(DESC_THE, true).c_str());
        return false;
    }

    const actor* landing_actor = actor_at(wall_jump_landing_spot);
    if (feat_is_solid(env.grid(you.pos() + wall_jump_direction))
        || !in_bounds(wall_jump_landing_spot)
        || !you.is_habitable(wall_jump_landing_spot)
        || landing_actor)
    {
        if (landing_actor)
        {
            error_ret = make_stringf(
                "You have no room to wall jump there; %s is in the way.",
                landing_actor->observable()
                            ? landing_actor->name(DESC_THE).c_str()
                            : "something you can't see");
        }
        else
            error_ret = "You have no room to wall jump there.";
        return false;
    }
    error_ret = "";
    return true;
}

/**
 * Do a walljump.
 *
 * This doesn't check whether there's space; see `wu_jian_can_wall_jump`.
 * It does check whether the landing spot is safe, excluded, etc.
 *
 * @param targ the movement target (i.e. the wall being moved against).
 * @return whether the jump culminated.
 */
bool wu_jian_do_wall_jump(coord_def targ)
{
    // whether there's space in the first place is checked earlier
    // in wu_jian_can_wall_jump.
    auto wall_jump_direction = (you.pos() - targ).sgn();
    auto wall_jump_landing_spot = (you.pos() + wall_jump_direction
                                   + wall_jump_direction);
    if (!check_moveto(wall_jump_landing_spot, "wall jump"))
    {
        you.turn_is_over = false;
        return false;
    }

    auto initial_position = you.pos();
    move_player_to_grid(wall_jump_landing_spot, false);
    wu_jian_wall_jump_effects();
    remove_water_hold();

    int wall_jump_modifier = (you.attribute[ATTR_SERPENTS_LASH] != 1) ? 2
                                                                      : 1;

    you.time_taken = player_speed() * wall_jump_modifier
                     * player_movement_speed();
    you.time_taken = div_rand_round(you.time_taken, 10);

    // need to set this here in case serpent's lash isn't active
    you.turn_is_over = true;
    request_autopickup();
    wu_jian_post_move_effects(true, initial_position);

    return true;
}

spret wu_jian_wall_jump_ability()
{
    ASSERT(!crawl_state.game_is_arena());

    if (crawl_state.is_replaying_keys())
    {
        crawl_state.cancel_cmd_all("You can't repeat a wall jump.");
        return spret::abort;
    }

    if (you.digging)
    {
        you.digging = false;
        mpr("You retract your mandibles.");
    }

    string wj_error;
    bool has_targets = false;

    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (wu_jian_can_wall_jump(*ai, wj_error))
        {
            has_targets = true;
            break;
        }

    if (!has_targets)
    {
        mpr("There is nothing to wall jump against here.");
        return spret::abort;
    }

    if (you.is_nervous())
    {
        mpr("You are too terrified to wall jump!");
        return spret::abort;
    }

    if (you.attribute[ATTR_HELD])
    {
        mprf("You cannot wall jump while caught in a %s.",
             get_trapping_net(you.pos()) == NON_ITEM ? "web" : "net");
        return spret::abort;
    }

    if (!you.attempt_escape())
        return spret::fail;

    // query for location:
    dist beam;

    while (1)
    {
        direction_chooser_args args;
        args.restricts = DIR_TARGET;
        args.mode = TARG_ANY;
        args.range = 1;
        args.needs_path = false; // TODO: overridden by hitfunc?
        args.top_prompt = "Aiming: <white>Wall Jump</white>";
        args.self = confirm_prompt_type::cancel;
        targeter_walljump tgt;
        tgt.obeys_mesmerise = true;
        args.hitfunc = &tgt;
        {
            // TODO: make this unnecessary
            direction_chooser dc(beam, args);
            dc.needs_path = false;
            dc.choose_direction();
        }
        if (crawl_state.seen_hups)
        {
            clear_messages();
            mpr("Cancelling wall jump due to HUP.");
            return spret::abort;
        }

        if (!beam.isValid || beam.target == you.pos())
        {
            canned_msg(MSG_OK);
            return spret::abort; // early return
        }

        if (wu_jian_can_wall_jump(beam.target, wj_error))
            break;
    }

    if (!wu_jian_do_wall_jump(beam.target))
        return spret::abort;

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    apply_barbs_damage();
    return spret::success;
}

void wu_jian_heavenly_storm()
{
    mprf(MSGCH_GOD, "The air is filled with shimmering golden clouds!");
    wu_jian_sifu_message(" says: The storm will not cease as long as you "
                         "keep fighting, disciple!");

    for (radius_iterator ai(you.pos(), 2, C_SQUARE, LOS_SOLID); ai; ++ai)
        if (!cell_is_solid(*ai))
            place_cloud(CLOUD_GOLD_DUST, *ai, 5 + random2(5), &you);

    you.set_duration(DUR_HEAVENLY_STORM, random_range(2, 3));
    you.props[WU_JIAN_HEAVENLY_STORM_KEY] = WU_JIAN_HEAVENLY_STORM_INITIAL;
    invalidate_agrid(true);
}

bool okawaru_duel_active()
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->props.exists(OKAWARU_DUEL_CURRENT_KEY))
            return true;
    }

    return false;
}

spret okawaru_duel(const coord_def& target, bool fail)
{
    monster* mons = monster_at(target);
    if (!mons || !you.can_see(*mons))
    {
        mpr("You can see no monster there to duel!");
        return spret::abort;
    }

    if (mons_is_firewood(*mons)
        || mons_is_conjured(mons->type)
        || mons_is_tentacle_or_tentacle_segment(mons->type)
        || mons_primary_habitat(*mons) == HT_LAVA
        || mons_primary_habitat(*mons) == HT_WATER
        || mons->wont_attack())
    {
        mpr("You cannot duel that!");
        return spret::abort;
    }

    if (mons_threat_level(*mons) < MTHRT_TOUGH)
    {
        simple_monster_message(*mons, " is not worthy to be dueled!");
        return spret::abort;
    }

    if (mons->is_illusion())
    {
        fail_check();
        mprf("You challenge %s to single combat, but %s is merely a clone!",
             mons->name(DESC_THE).c_str(),
             mons->pronoun(PRONOUN_SUBJECTIVE).c_str());
        // Still costs a turn to gain the information.
        return spret::success;
    }
    // Check this after everything else so as not to waste a turn when trying
    // to duel a clone that's already invalid to be dueled for other reasons.
    else if (mons->is_summoned())
    {
        mpr("You cannot duel that!");
        return spret::abort;
    }

    fail_check();

    mprf("You enter into single combat with %s!",
         mons->name(DESC_THE).c_str());

    behaviour_event(mons, ME_ALERT, &you);
    mons->props[OKAWARU_DUEL_TARGET_KEY] = true;
    mons->props[OKAWARU_DUEL_CURRENT_KEY] = true;
    mons->stop_being_constricted();
    mons->set_transit(level_id(BRANCH_ARENA));
    mons->destroy_inventory();
    if (mons_is_elven_twin(mons))
        elven_twin_died(mons, false, KILL_YOU, MID_PLAYER);
    monster_cleanup(mons);

    stop_delay(true);
    down_stairs(DNGN_ENTER_ARENA);

    return spret::success;
}

void okawaru_remove_heroism()
{
    mprf(MSGCH_DURATION, "You feel like a meek peon again.");
    you.duration[DUR_HEROISM] = 0;
    you.redraw_evasion      = true;
    you.redraw_armour_class = true;
}

void okawaru_remove_finesse()
{
    mprf(MSGCH_DURATION, "%s", you.hands_act("slow", "down.").c_str());
    you.duration[DUR_FINESSE] = 0;
}

// End a duel, and send the duel target back with the player if it's still
// alive.
void okawaru_end_duel()
{
    ASSERT(player_in_branch(BRANCH_ARENA));
    if (okawaru_duel_active())
    {
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->props.exists(OKAWARU_DUEL_CURRENT_KEY))
            {
                mi->props.erase(OKAWARU_DUEL_CURRENT_KEY);
                mi->props[OKAWARU_DUEL_ABANDONED_KEY] = true;
                mi->stop_being_constricted();
                mi->set_transit(current_level_parent());
                mi->destroy_inventory();
                monster_cleanup(*mi);
            }
        }
    }

    mpr("You are returned from the Arena.");
    stop_delay(true);
    down_stairs(DNGN_EXIT_ARENA);
}

vector<coord_def> find_slimeable_walls()
{
    vector<coord_def> walls;

    for (radius_iterator ri(you.pos(), 4, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (feat_is_wall(env.grid(*ri))
            && !feat_is_permarock(env.grid(*ri))
            && env.grid(*ri) != DNGN_SLIMY_WALL)
        {
            walls.push_back(*ri);
        }
    }

    return walls;
}

spret jiyva_oozemancy(bool fail)
{
    vector<coord_def> walls = find_slimeable_walls();

    if (walls.empty())
    {
        mpr("There are no walls around you to affect.");
        return spret::abort;
    }

    fail_check();

    const int dur = 10 + random2avg(you.piety / 8, 2);

    for (auto pos : walls)
    {
        temp_change_terrain(pos, DNGN_SLIMY_WALL, dur * BASELINE_DELAY,
                            TERRAIN_CHANGE_SLIME);
    }

    you.increase_duration(DUR_OOZEMANCY, dur);
    mpr("Slime begins to ooze from the nearby walls!");

    return spret::success;
}

void jiyva_end_oozemancy()
{
    you.duration[DUR_OOZEMANCY] = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
        if (env.grid(*ri) == DNGN_SLIMY_WALL && is_temp_terrain(*ri))
            revert_terrain_change(*ri, TERRAIN_CHANGE_SLIME);
}

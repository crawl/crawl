/**
 * @file
 * @brief Spell casting and miscast functions.
**/

#include "AppHdr.h"

#include "spl-cast.h"

#include <iomanip>
#include <sstream>
#include <cmath>

#include "areas.h"
#include "art-enum.h"
#include "beam.h"
#include "chardump.h"
#include "colour.h"
#include "database.h"
#include "describe.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "exercise.h"
#include "format.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::shadow_spells
#include "hints.h"
#include "item-prop.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-util.h"
#include "mutation.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-monench.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-wpnench.h"
#include "spl-zap.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "tilepick.h"
#include "transform.h"
#include "unicode.h"
#include "unwind.h"
#include "view.h"
#include "viewchar.h" // stringize_glyph

static int _spell_enhancement(spell_type spell);
static string _spell_failure_rate_description(spell_type spell);

#if TAG_MAJOR_VERSION == 34
void surge_power(const int enhanced)
{
    if (enhanced)               // one way or the other {dlb}
    {
        const string modifier = (enhanced  < -2) ? "extraordinarily" :
                                (enhanced == -2) ? "extremely" :
                                (enhanced ==  2) ? "strong" :
                                (enhanced  >  2) ? "huge"
                                                 : "";
        mprf("You feel %s %s",
             !modifier.length() ? "a"
                                : article_a(modifier).c_str(),
             (enhanced < 0) ? "numb sensation."
                            : "surge of power!");
    }
}

void surge_power_wand(const int mp_cost)
{
    if (mp_cost)
    {
        const bool slight = mp_cost < 3;
        mprf("You feel a %ssurge of power%s",
             slight ? "slight " : "",
             slight ? "."      : "!");
    }
}
#endif

static string _spell_base_description(spell_type spell, bool viewing)
{
    ostringstream desc;

    int highlight =  spell_highlight_by_utility(spell, COL_UNKNOWN, !viewing);

    desc << "<" << colour_to_str(highlight) << ">" << left;

    // spell name
    desc << chop_string(spell_title(spell), 30);

    // spell schools
    desc << spell_schools_string(spell);

    const int so_far = strwidth(desc.str()) - (strwidth(colour_to_str(highlight))+2);
    if (so_far < 60)
        desc << string(60 - so_far, ' ');
    desc << "</" << colour_to_str(highlight) <<">";

    // spell fail rate, level
    const string failure_rate = spell_failure_rate_string(spell);
    const int width = strwidth(formatted_string::parse_string(failure_rate).tostring());
    desc << failure_rate << string(12-width, ' ');
    desc << spell_difficulty(spell);
    desc << " ";

    return desc.str();
}

static string _spell_extra_description(spell_type spell, bool viewing)
{
    ostringstream desc;

    int highlight =  spell_highlight_by_utility(spell, COL_UNKNOWN, !viewing);

    desc << "<" << colour_to_str(highlight) << ">" << left;

    // spell name
    desc << chop_string(spell_title(spell), 30);

    // spell power, spell range, noise
    const string rangestring = spell_range_string(spell);

    desc << chop_string(spell_power_string(spell), 13)
         << chop_string(rangestring, 9)
         << chop_string(spell_noise_string(spell, 10), 14);

    desc << "</" << colour_to_str(highlight) <<">";

    return desc.str();
}

// selector is a boolean function that filters spells according
// to certain criteria. Currently used for Tiles to distinguish
// spells targeted on player vs. spells targeted on monsters.
int list_spells(bool toggle_with_I, bool viewing, bool allow_preselect,
                const string &title, spell_selector selector)
{
    if (toggle_with_I && get_spell_by_letter('I') != SPELL_NO_SPELL)
        toggle_with_I = false;

    ToggleableMenu spell_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
            | MF_NO_WRAP_ROWS | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING);
    string titlestring = make_stringf("%-25.25s", title.c_str());
    {
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(
                titlestring + "         Type                          Failure  Level",
                titlestring + "         Power        Range    Noise         ",
                MEL_TITLE);
        spell_menu.set_title(me, true, true);
    }
    spell_menu.set_highlighter(nullptr);
    spell_menu.set_tag("spell");
    spell_menu.add_toggle_key('!');

    string more_str = "Press '<w>!</w>' ";
    if (toggle_with_I)
    {
        spell_menu.add_toggle_key('I');
        more_str += "or '<w>I</w>' ";
    }
    if (!viewing)
        spell_menu.menu_action = Menu::ACT_EXECUTE;
    more_str += "to toggle spell view.";
    spell_menu.set_more(formatted_string::parse_string(more_str));

    // If there's only a single spell in the offered spell list,
    // taking the selector function into account, preselect that one.
    bool preselect_first = false;
    if (allow_preselect)
    {
        int count = 0;
        if (you.spell_no == 1)
            count = 1;
        else if (selector)
        {
            for (int i = 0; i < 52; ++i)
            {
                const char letter = index_to_letter(i);
                const spell_type spell = get_spell_by_letter(letter);
                if (!is_valid_spell(spell) || !(*selector)(spell))
                    continue;

                // Break out early if we've got > 1 spells.
                if (++count > 1)
                    break;
            }
        }
        // Preselect the first spell if it's only spell applicable.
        preselect_first = (count == 1);
    }
    if (allow_preselect || preselect_first
                           && you.last_cast_spell != SPELL_NO_SPELL)
    {
        spell_menu.set_flags(spell_menu.get_flags() | MF_PRESELECTED);
    }

    for (int i = 0; i < 52; ++i)
    {
        const char letter = index_to_letter(i);
        const spell_type spell = get_spell_by_letter(letter);

        if (!is_valid_spell(spell))
            continue;

        if (selector && !(*selector)(spell))
            continue;

        bool preselect = (preselect_first
                          || allow_preselect && you.last_cast_spell == spell);

        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(_spell_base_description(spell, viewing),
                                    _spell_extra_description(spell, viewing),
                                    MEL_ITEM, 1, letter, preselect);

        me->add_tile(tile_def(tileidx_spell(spell)));
        spell_menu.add_entry(me);
    }

    int choice = 0;
    spell_menu.on_single_selection = [&choice, &spell_menu](const MenuEntry& item)
    {
        ASSERT(item.hotkeys.size() == 1);
        if (spell_menu.menu_action == Menu::ACT_EXAMINE)
        {
            describe_spell(get_spell_by_letter(item.hotkeys[0]), nullptr);
            return true;
        }
        else
        {
            choice = item.hotkeys[0];
            return false;
        }
    };

    spell_menu.show();
    if (!crawl_state.doing_prev_cmd_again)
    {
        redraw_screen();
        update_screen();
    }
    return choice;
}

static int _apply_spellcasting_success_boosts(spell_type spell, int chance)
{
    int fail_reduce = 100;

    if (have_passive(passive_t::spells_success) && vehumet_supports_spell(spell))
    {
        // [dshaligram] Fail rate multiplier used to be .5, scaled
        // back to 67%.
        fail_reduce = fail_reduce * 2 / 3;
    }

    const int wizardry = player_wizardry(spell);

    if (wizardry > 0)
      fail_reduce = fail_reduce * 6 / (7 + wizardry);

    // Hard cap on fail rate reduction.
    if (fail_reduce < 50)
        fail_reduce = 50;

    return chance * fail_reduce / 100;
}

/**
 * Calculate the player's failure rate with the given spell, including all
 * modifiers. (Armour, mutations, statuses effects, etc.)
 *
 * @param spell     The spell in question.
 * @return          A failure rate. This is *not* a percentage - for a human-
 *                  readable version, call _get_true_fail_rate().
 */
int raw_spell_fail(spell_type spell)
{
    int chance = 60;

    // Don't cap power for failure rate purposes.
    // scale by 6, which I guess was chosen because it seems to work.
    // realistic range for spellpower: -6 to -366 (before scale -1 to -61)
    chance -= calc_spell_power(spell, false, true, false, 6);
    chance -= (you.intel() * 2); // realistic range: -2 to -70

    const int armour_shield_penalty = player_armour_shield_spell_penalty();
    dprf("Armour+Shield spell failure penalty: %d", armour_shield_penalty);
    chance += armour_shield_penalty; // range: 0 to 500 in extreme cases.
                                     // A midlevel melee character in plate
                                     // might have 40 or 50, and a caster in a
                                     // robe would usually have 0.

    static const int difficulty_by_level[] =
    {
        0,
        3,
        15,
        35,

        70,
        100,
        150,

        200,
        260,
        340,
    };
    const int spell_level = spell_difficulty(spell);
    ASSERT_RANGE(spell_level, 0, (int) ARRAYSZ(difficulty_by_level));
    chance += difficulty_by_level[spell_level]; // between 0 and 330

    // since chance is passed through a 3rd degree polynomial, cap the
    // value to avoid any overflow issues. We choose 210 by solving for chance2
    // = 200 in the polynomial -- it gets capped at 100 ultimately, but we
    // need a bunch of headroom in case some later calculations lower the value
    // below 100 after this.
    chance = min(chance, 210);

    // This polynomial is a smoother approximation of a breakpoint-based
    // calculation that originates pre-DCSS, mapping `chance` at this point to
    // values from around 0 to around 45. (see
    // https://crawl.develz.org/tavern/viewtopic.php?f=8&t=23414 for some of
    // the history.)  It was calculated by |amethyst (based on one from minmay
    // in that thread) and converted to integer values using 262144 as a
    // convenient power of 2 denominator, then converted to its current form
    // by Horner's rule, and then tweaked slightly.
    //
    // The regular (integer) polynomial form before Horner's rule is:
    //          (x*x*x + 426*x*x + 82670*x + 7245398) / 262144
    //
    // https://www.wolframalpha.com/input/?i=graph+of+y%3D(((x+%2B+426)*x+%2B+82670)*x+%2B+7245398)+%2F+262144+and+y%3D100+and+x%3D125.1+with+x+from+-192+to+126.1
    //
    // If you think this is weird, you should see what was here before.
    int chance2 = max((((chance + 426) * chance + 82670) * chance + 7245398)
                      / 262144, 0);

    chance2 += get_form()->spellcasting_penalty;
    if (you.duration[DUR_EXCRUCIATING_WOUNDS])
        chance2 += 10; // same as spider form

    chance2 -= 2 * you.get_mutation_level(MUT_SUBDUED_MAGIC);
    chance2 += 4 * you.get_mutation_level(MUT_WILD_MAGIC);
    chance2 += 4 * you.get_mutation_level(MUT_ANTI_WIZARDRY);

    if (you.props.exists(SAP_MAGIC_KEY))
        chance2 += you.props[SAP_MAGIC_KEY].get_int() * 12;

    chance2 += you.duration[DUR_VERTIGO] ? 7 : 0;

    // Apply the effects of Vehumet and items of wizardry.
    chance2 = _apply_spellcasting_success_boosts(spell, chance2);

    return min(max(chance2, 0), 100);
}

/*
 * Given some spellpower in centis, do a stepdown at around 50 (5000 in centis)
 * and return a rescaled value.
 *
 * @param power the input spellpower in centis.
 * @param scale a value to scale the result by, between 1 and 1000. Default is
 *        1, which returns a regular spellpower. 1000 gives you millis, 100
 *        centis.
 */
int stepdown_spellpower(int power, int scale)
{
    // use millis internally
    ASSERT_RANGE(scale, 1, 1000);
    const int divisor = 1000 / scale;
    int result = stepdown_value(power * 10, 50000, 50000, 150000, 200000)
                    / divisor;
    return result;
}

/*
 * Calculate spell power.
 *
 * @param spell         the spell to check
 * @param apply_intel   whether to include intelligence in the calculation
 * @param fail_rate_check is this just a plain failure rate check or should it
 *                      incorporate situational facts and mutations?
 * @param cap_power     whether to apply the power cap for the spell (from
 *                      `spell_power_cap(spell)`)
 * @param scale         what scale to apply to the result internally?  This
 *                      function has higher internal resolution than the default
 *                      argument, so use this rather than dividing. This must be
 *                      between 1 and 1000.
 *
 * @return the resulting spell power.
 */
int calc_spell_power(spell_type spell, bool apply_intel, bool fail_rate_check,
                     bool cap_power, int scale)
{
    int power = 0;

    const spschools_type disciplines = get_spell_disciplines(spell);

    int skillcount = count_bits(disciplines);
    if (skillcount)
    {
        for (const auto bit : spschools_type::range())
            if (disciplines & bit)
                power += you.skill(spell_type2skill(bit), 200);
        power /= skillcount;
    }

    power += you.skill(SK_SPELLCASTING, 50);

    if (you.divine_exegesis)
        power += you.skill(SK_INVOCATIONS, 300);

    if (fail_rate_check)
    {
        // Scale appropriately.
        // The stepdown performs this step in the else block.
        power *= scale;
        power /= 100;
    }
    else
    {
        if (apply_intel)
            power = (power * you.intel()) / 10;

        // [dshaligram] Enhancers don't affect fail rates any more, only spell
        // power. Note that this does not affect Vehumet's boost in castability.
        power = apply_enhancement(power, _spell_enhancement(spell));

        // Wild magic boosts spell power but decreases success rate.
        power *= (10 + 3 * you.get_mutation_level(MUT_WILD_MAGIC));
        power /= (10 + 3 * you.get_mutation_level(MUT_SUBDUED_MAGIC));

        // Augmentation boosts spell power at high HP.
        power *= 10 + 4 * augmentation_amount();
        power /= 10;

        // Each level of horror reduces spellpower by 10%
        if (you.duration[DUR_HORROR])
        {
            power *= 10;
            power /= 10 + (you.props[HORROR_PENALTY_KEY].get_int() * 3) / 2;
        }

        // at this point, `power` is assumed to be basically in centis.
        // apply a stepdown, and scale.
        power = stepdown_spellpower(power, scale);
    }

    const int cap = spell_power_cap(spell);
    if (cap > 0 && cap_power)
        power = min(power, cap * scale);

    return power;
}

static int _spell_enhancement(spell_type spell)
{
    const spschools_type typeflags = get_spell_disciplines(spell);
    int enhanced = 0;

    if (typeflags & spschool::conjuration)
        enhanced += player_spec_conj();

    if (typeflags & spschool::hexes)
        enhanced += player_spec_hex();

    if (typeflags & spschool::summoning)
        enhanced += player_spec_summ();

    if (typeflags & spschool::poison)
        enhanced += player_spec_poison();

    if (typeflags & spschool::necromancy)
        enhanced += player_spec_death();

    if (typeflags & spschool::fire)
        enhanced += player_spec_fire();

    if (typeflags & spschool::ice)
        enhanced += player_spec_cold();

    if (typeflags & spschool::earth)
        enhanced += player_spec_earth();

    if (typeflags & spschool::air)
        enhanced += player_spec_air();

    if (you.wearing_ego(EQ_CLOAK, SPARM_SHADOWS))
        enhanced -= 1;

    if (you.form == transformation::shadow)
        enhanced -= 2;

    enhanced += you.archmagi();
    enhanced += player_equip_unrand(UNRAND_MAJIN);
    enhanced += you.duration[DUR_BRILLIANCE] > 0;

    // These are used in an exponential way, so we'll limit them a bit. -- bwr
    if (enhanced > 3)
        enhanced = 3;
    else if (enhanced < -3)
        enhanced = -3;

    return enhanced;
}

/**
 * Apply the effects of spell enhancers (and de-enhancers) on spellpower.
 *
 * @param initial_power     The power of the spell before enhancers are added.
 * @param enhancer_levels   The number of enhancements levels to apply.
 * @return                  The power of the spell with enhancers considered.
 */
int apply_enhancement(const int initial_power, const int enhancer_levels)
{
    int power = initial_power;

    if (enhancer_levels > 0)
    {
        for (int i = 0; i < enhancer_levels; i++)
        {
            power *= 15;
            power /= 10;
        }
    }
    else if (enhancer_levels < 0)
    {
        for (int i = enhancer_levels; i < 0; i++)
            power /= 2;
    }

    return power;
}

void inspect_spells()
{
    if (!you.spell_no)
    {
        canned_msg(MSG_NO_SPELLS);
        return;
    }

    list_spells(true, true);
}

/**
 * Can the player cast any spell at all? Checks for things that limit
 * spellcasting regardless of the specific spell we want to cast.
 *
 * @param quiet    If true, don't print a reason why no spell can be cast.
 * @param exegesis If true, we're considering casting under Divine Exegesis.
 * @return True if we could cast a spell, false otherwise.
*/
bool can_cast_spells(bool quiet, bool exegesis)
{
    if (!get_form()->can_cast)
    {
        if (!quiet)
            canned_msg(MSG_PRESENT_FORM);
        return false;
    }

    if (you.duration[DUR_WATER_HOLD] && !you.res_water_drowning())
    {
        if (!quiet)
            mpr("You cannot cast spells while unable to breathe!");
        return false;
    }

    if (you.duration[DUR_BRAINLESS])
    {
        if (!quiet)
            mpr("You lack the mental capacity to cast spells.");
        return false;
    }

    // Randart weapons.
    if (you.no_cast())
    {
        if (!quiet)
            mpr("Something interferes with your magic!");
        return false;
    }

    // Check that we have a spell memorised. Divine Exegesis does not need this
    // condition, but we can't just check you.divine_exegesis in all cases, as
    // it may not be set yet. Check a passed parameter instead.
    if (!exegesis && !you.spell_no)
    {
        if (!quiet)
            canned_msg(MSG_NO_SPELLS);
        return false;
    }

    if (you.berserk())
    {
        if (!quiet)
            canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (you.confused())
    {
        if (!quiet)
            mpr("You're too confused to cast spells.");
        return false;
    }

    if (silenced(you.pos()))
    {
        if (!quiet)
            mpr("You cannot cast spells when silenced!");
        // included in default force_more_message
        return false;
    }

    return true;
}

void do_cast_spell_cmd(bool force)
{
    if (!cast_a_spell(!force))
        flush_input_buffer(FLUSH_ON_FAILURE);
}

/**
 * Cast a spell.
 *
 * Handles general preconditions & costs.
 *
 * @param check_range   If true, abort if no targets are in range. (z vs Z)
 * @param spell         The type of spell to be cast.
 * @return              Whether the spell was successfully cast.
 **/
bool cast_a_spell(bool check_range, spell_type spell)
{
    if (!can_cast_spells(false, you.divine_exegesis))
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    if (crawl_state.game_is_hints())
        Hints.hints_spell_counter++;

    if (spell == SPELL_NO_SPELL)
    {
        int keyin = 0;

        string luachoice;
        if (!clua.callfn("c_choose_spell", ">s", &luachoice))
        {
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
        }
        else if (!luachoice.empty() && isalpha(luachoice[0]))
        {
            keyin = luachoice[0];
            const spell_type spl = get_spell_by_letter(keyin);

            // Bad entry from lua, defer to the user
            if (!is_valid_spell(spl))
                keyin = 0;
        }

        while (true)
        {
#ifdef TOUCH_UI
            keyin = list_spells(true, false);
            if (!keyin)
                keyin = ESCAPE;

            if (!crawl_state.doing_prev_cmd_again)
            {
                redraw_screen();
                update_screen();
            }

            if (isaalpha(keyin) || key_is_escape(keyin))
                break;
            else
                clear_messages();

            keyin = 0;
#else
            if (keyin == 0)
            {
                if (you.spell_no == 1)
                {
                    // Set last_cast_spell to the current only spell.
                    for (int i = 0; i < 52; ++i)
                    {
                        const char letter = index_to_letter(i);
                        const spell_type spl = get_spell_by_letter(letter);

                        if (!is_valid_spell(spl))
                            continue;

                        you.last_cast_spell = spl;
                        break;
                    }
                }

                // We allow setting last cast spell by Divine Exegesis, but we
                // don't allow recasting it with the UI unless we actually have
                // the spell memorized.
                if (you.last_cast_spell != SPELL_NO_SPELL
                    && !you.has_spell(you.last_cast_spell))
                {
                    you.last_cast_spell = SPELL_NO_SPELL;
                }

                if (you.last_cast_spell == SPELL_NO_SPELL
                    || !Options.enable_recast_spell)
                {
                    mprf(MSGCH_PROMPT, "Cast which spell? (? or * to list) ");
                }
                else
                {
                    mprf(MSGCH_PROMPT, "Casting: <w>%s</w> <lightgrey>(%s)</lightgrey>",
                                       spell_title(you.last_cast_spell),
                                       _spell_failure_rate_description(you.last_cast_spell).c_str());
                    mprf(MSGCH_PROMPT, "Confirm with . or Enter, or press "
                                       "? or * to list all spells.");
                }

                keyin = get_ch();
            }

            if (keyin == '?' || keyin == '*')
            {
                keyin = list_spells(true, false);
                if (!keyin)
                    keyin = ESCAPE;

                if (!crawl_state.doing_prev_cmd_again)
                {
                    redraw_screen();
                    update_screen();
                }

                if (isaalpha(keyin) || key_is_escape(keyin))
                    break;
                else
                    clear_messages();

                keyin = 0;
            }
            else
                break;
#endif
        }

        if (key_is_escape(keyin))
        {
            canned_msg(MSG_OK);
            crawl_state.zero_turns_taken();
            return false;
        }
        else if (Options.enable_recast_spell
                 && (keyin == '.' || keyin == CK_ENTER))
        {
            spell = you.last_cast_spell;
        }
        else if (!isaalpha(keyin))
        {
            mpr("You don't know that spell.");
            crawl_state.zero_turns_taken();
            return false;
        }
        else
            spell = get_spell_by_letter(keyin);
    }

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        crawl_state.zero_turns_taken();
        return false;
    }

    int cost = spell_mana(spell);
    if (!enough_mp(cost, true))
    {
        mpr("You don't have enough magic to cast that spell.");
        crawl_state.zero_turns_taken();
        return false;
    }

    if (check_range && spell_no_hostile_in_range(spell))
    {
        // Abort if there are no hostiles within range, but flash the range
        // markers for a short while.
        mpr("You can't see any susceptible monsters within range! "
            "(Use <w>Z</w> to cast anyway.)");

        if ((Options.use_animations & UA_RANGE) && Options.darken_beyond_range)
        {
            targeter_smite range(&you, calc_spell_range(spell), 0, 0, true);
            range_view_annotator show_range(&range);
            delay(50);
        }
        crawl_state.zero_turns_taken();
        return false;
    }

    // This needs more work: there are spells which are hated but allowed if
    // they don't have a certain effect. You may use Poison Arrow on those
    // immune, use Mephitic Cloud to shield yourself from other clouds, and
    // thus we don't prompt for them. It would be nice to prompt for them
    // during the targeting phase, perhaps.
    if (god_punishes_spell(spell, you.religion)
        && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        // None currently dock just piety, right?
        if (!yesno(god_loathes_spell(spell, you.religion) ?
            "Casting this spell will cause instant excommunication! "
            "Really cast?" :
            "Casting this spell will place you under penance. Really cast?",
            true, 'n'))
        {
            canned_msg(MSG_OK);
            crawl_state.zero_turns_taken();
            return false;
        }
    }

    you.last_cast_spell = spell;
    // Silently take MP before the spell.
    dec_mp(cost, true);

    const spret cast_result = your_spells(spell, 0, !you.divine_exegesis,
                                          nullptr);
    if (cast_result == spret::abort)
    {
        crawl_state.zero_turns_taken();
        // Return the MP since the spell is aborted.
        inc_mp(cost, true);
        redraw_screen();
        update_screen();
        return false;
    }

    practise_casting(spell, cast_result == spret::success);
    if (cast_result == spret::success)
    {
        did_god_conduct(DID_SPELL_CASTING, 1 + random2(5));
        count_action(CACT_CAST, spell);
    }

    flush_mp();

    you.turn_is_over = true;
    alert_nearby_monsters();

    return true;
}

/**
 * Handles divine response to spellcasting.
 *
 * @param spell         The type of spell just cast.
 */
static void _spellcasting_god_conduct(spell_type spell)
{
    // If you are casting while a god is acting, then don't do conducts.
    // (Presumably Xom is forcing you to cast a spell.)
    if (crawl_state.is_god_acting())
        return;

    const int conduct_level = 10 + spell_difficulty(spell);

    if (is_evil_spell(spell) || you.spellcasting_unholy())
        did_god_conduct(DID_EVIL, conduct_level);

    if (is_unclean_spell(spell))
        did_god_conduct(DID_UNCLEAN, conduct_level);

    if (is_chaotic_spell(spell))
        did_god_conduct(DID_CHAOS, conduct_level);

    // not is_hasty_spell since the other ones handle the conduct themselves.
    if (spell == SPELL_SWIFTNESS)
        did_god_conduct(DID_HASTY, conduct_level);

    if (spell == SPELL_SUBLIMATION_OF_BLOOD)
        did_god_conduct(DID_CHANNEL, conduct_level);

    if (god_loathes_spell(spell, you.religion))
        excommunication();
}

/**
 * Let the Majin-Bo congratulate you on casting a spell while using it.
 *
 * @param spell     The spell just successfully cast.
 */
static void _majin_speak(spell_type spell)
{
    // since this isn't obviously mental communication, let it be silenced
    if (silenced(you.pos()))
        return;

    const int level = spell_difficulty(spell);
    const bool weak = level <= 4;
    const string lookup = weak ? "majin-bo cast weak" : "majin-bo cast";
    const string msg = "A voice whispers, \"" + getSpeakString(lookup) + "\"";
    mprf(MSGCH_TALK, "%s", msg.c_str());
}

/**
 * Handles side effects of successfully casting a spell.
 *
 * Spell noise, magic 'sap' effects, and god conducts.
 *
 * @param spell         The type of spell just cast.
 * @param god           Which god is casting the spell; NO_GOD if it's you.
 * @param fake_spell    true if the spell is evoked or from an innate or divine ability
 *                      false if it is a spell being cast normally.
 */
static void _spellcasting_side_effects(spell_type spell, god_type god,
                                       bool fake_spell)
{
    _spellcasting_god_conduct(spell);

    if (god == GOD_NO_GOD)
    {
        // Casting pain costs 1 hp.
        // Deep Dwarves' damage reduction always blocks at least 1 hp.
        if (spell == SPELL_PAIN
            && (you.species != SP_DEEP_DWARF && !player_res_torment()))
        {
            dec_hp(1, false);
        }

        if (you.duration[DUR_SAP_MAGIC]
            && you.props[SAP_MAGIC_KEY].get_int() < 3
            && !fake_spell && coinflip())
        {
            mprf(MSGCH_WARN, "Your control over your magic is sapped.");
            you.props[SAP_MAGIC_KEY].get_int()++;
        }

        // Make some noise if it's actually the player casting.
        noisy(spell_noise(spell), you.pos());

        if (!fake_spell && player_equip_unrand(UNRAND_MAJIN))
        {
            // never kill the player (directly)
            int hp_cost = min(spell_mana(spell), you.hp - 1);
            ouch(hp_cost, KILLED_BY_SOMETHING, MID_NOBODY, "the Majin-Bo");
            if (one_chance_in(500))
                _majin_speak(spell);
        }
    }

    alert_nearby_monsters();

}

#ifdef WIZARD
static void _try_monster_cast(spell_type spell, int /*powc*/,
                              dist &spd, bolt &beam)
{
    if (monster_at(you.pos()))
    {
        mpr("Couldn't try casting monster spell because you're "
            "on top of a monster.");
        return;
    }

    monster* mon = get_free_monster();
    if (!mon)
    {
        mpr("Couldn't try casting monster spell because there is "
            "no empty monster slot.");
        return;
    }

    mpr("Invalid player spell, attempting to cast it as monster spell.");

    mon->mname      = "Dummy Monster";
    mon->type       = MONS_HUMAN;
    mon->behaviour  = BEH_SEEK;
    mon->attitude   = ATT_FRIENDLY;
    mon->flags      = (MF_NO_REWARD | MF_JUST_SUMMONED | MF_SEEN
                       | MF_WAS_IN_VIEW | MF_HARD_RESET);
    mon->hit_points = you.hp;
    mon->set_hit_dice(you.experience_level);
    mon->set_position(you.pos());
    mon->target     = spd.target;
    mon->mid        = MID_PLAYER;

    if (!spd.isTarget)
        mon->foe = MHITNOT;
    else if (!monster_at(spd.target))
    {
        if (spd.isMe())
            mon->foe = MHITYOU;
        else
            mon->foe = MHITNOT;
    }
    else
        mon->foe = mgrd(spd.target);

    mgrd(you.pos()) = mon->mindex();

    mons_cast(mon, beam, spell, MON_SPELL_NO_FLAGS);

    mon->reset();
}
#endif // WIZARD

static spret _do_cast(spell_type spell, int powc, const dist& spd,
                           bolt& beam, god_type god, bool fail);

/**
 * Should this spell be aborted before casting properly starts, either because
 * it can't legally be cast in this circumstance, or because the player opts
 * to cancel it in response to a prompt?
 *
 * @param spell         The spell to be checked
 * @param fake_spell    true if the spell is evoked or from an innate or divine ability
 *                      false if it is a spell being cast normally.
 * @return              Whether the spellcasting should be aborted.
 */
static bool _spellcasting_aborted(spell_type spell, bool fake_spell)
{
    string msg;

    {
        // FIXME: we might be called in a situation ([a]bilities, Xom) that
        // isn't evoked but still doesn't use the spell's MP. your_spells,
        // this function, and spell_uselessness_reason should take a flag
        // indicating whether MP should be checked (or should never check).
        const int rest_mp = fake_spell ? 0 : spell_mana(spell);

        // Temporarily restore MP so that we're not uncastable for lack of MP.
        unwind_var<int> fake_mp(you.magic_points, you.magic_points + rest_mp);
        msg = spell_uselessness_reason(spell, true, true, fake_spell);
    }

    if (!msg.empty())
    {
        mpr(msg);
        return true;
    }

    vector<text_pattern> &actions = Options.confirm_action;
    if (!actions.empty())
    {
        const char* name = spell_title(spell);
        for (const text_pattern &action : actions)
        {
            if (!action.matches(name))
                continue;

            string prompt = "Really cast " + string(name) + "?";
            if (!yesno(prompt.c_str(), false, 'n'))
            {
                canned_msg(MSG_OK);
                return true;
            }
            break;
        }
    }

    const int severity = fail_severity(spell);
    const string failure_rate = spell_failure_rate_string(spell);
    if (Options.fail_severity_to_confirm > 0
        && Options.fail_severity_to_confirm <= severity
        && !crawl_state.disables[DIS_CONFIRMATIONS]
        && !fake_spell)
    {
        if (failure_rate_to_int(raw_spell_fail(spell)) == 100)
        {
            mprf(MSGCH_WARN, "It is impossible to cast this spell "
                    "(100%% risk of failure)!");
            return true;
        }

        string prompt = make_stringf("The spell is %s to cast "
                                     "(%s risk of failure)%s",
                                     fail_severity_adjs[severity],
                                     failure_rate.c_str(),
                                     severity > 1 ? "!" : ".");

        prompt = make_stringf("%s Continue anyway?", prompt.c_str());
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return true;
        }
    }

    return false;
}

static unique_ptr<targeter> _spell_targeter(spell_type spell, int pow,
                                              int range)
{
    switch (spell)
    {
    case SPELL_FIREBALL:
        return make_unique<targeter_beam>(&you, range, ZAP_FIREBALL, pow,
                                          1, 1);
    case SPELL_ICEBLAST:
        return make_unique<targeter_beam>(&you, range, ZAP_ICEBLAST, pow,
                                          1, 1);
    case SPELL_HURL_DAMNATION:
        return make_unique<targeter_beam>(&you, range, ZAP_DAMNATION, pow,
                                          1, 1);
    case SPELL_MEPHITIC_CLOUD:
        return make_unique<targeter_beam>(&you, range, ZAP_MEPHITIC, pow,
                                          pow >= 100 ? 1 : 0, 1);
    case SPELL_FIRE_STORM:
        return make_unique<targeter_smite>(&you, range, 2, pow > 76 ? 3 : 2);
    case SPELL_FREEZING_CLOUD:
    case SPELL_POISONOUS_CLOUD:
    case SPELL_HOLY_BREATH:
        return make_unique<targeter_cloud>(&you, range);
    case SPELL_THUNDERBOLT:
        return make_unique<targeter_thunderbolt>(&you, range,
            (you.props.exists(THUNDERBOLT_LAST_KEY)
             && you.props[THUNDERBOLT_LAST_KEY].get_int() + 1 == you.num_turns) ?
                you.props[THUNDERBOLT_AIM_KEY].get_coord() : coord_def());
    case SPELL_LRD:
        return make_unique<targeter_fragment>(&you, pow, range);
    case SPELL_FULMINANT_PRISM:
        return make_unique<targeter_smite>(&you, range, 0, 2);
    case SPELL_GLACIATE:
        return make_unique<targeter_cone>(&you, range);
    case SPELL_GRAVITAS:
        return make_unique<targeter_smite>(&you, range,
                                           gravitas_range(pow),
                                           gravitas_range(pow),
                                           false,
                                           [](const coord_def& p) -> bool {
                                              return you.pos() != p; });
    case SPELL_VIOLENT_UNRAVELLING:
        return make_unique<targeter_unravelling>(&you, range, pow);
    case SPELL_RANDOM_BOLT:
        return make_unique<targeter_beam>(&you, range, ZAP_CRYSTAL_BOLT, pow,
                                          0, 0);
    case SPELL_INFESTATION:
        return make_unique<targeter_smite>(&you, range, 2, 2, false,
                                           [](const coord_def& p) -> bool {
                                              return you.pos() != p; });
    case SPELL_PASSWALL:
        return make_unique<targeter_passwall>(range);
    case SPELL_DIG:
        return make_unique<targeter_dig>(range);
    default:
        break;
    }

    if (spell_to_zap(spell) != NUM_ZAPS)
    {
        return make_unique<targeter_beam>(&you, range, spell_to_zap(spell),
                                          pow, 0, 0);
    }

    return nullptr;
}

// Returns the nth triangular number.
static int _triangular_number(int n)
{
    return n * (n+1) / 2;
}

/**
 * Compute success chance for MR-checking spells and abilities.
 *
 * @param mr The magic resistance of the target.
 * @param powc The enchantment power.
 * @param scale The denominator of the result.
 * @param round_up Should the resulting chance be rounded up (true) or
 *        down (false, the default)?
 *
 * @return The chance, out of scale, that the enchantment affects the target.
 */
int hex_success_chance(const int mr, int powc, int scale, bool round_up)
{
    const int pow = ench_power_stepdown(powc);
    const int target = mr + 100 - pow;
    const int denom = 101 * 100;
    const int adjust = round_up ? denom - 1 : 0;

    if (target <= 0)
        return scale;
    if (target > 200)
        return 0;
    if (target <= 100)
        return (scale * (denom - _triangular_number(target)) + adjust) / denom;
    return (scale * _triangular_number(201 - target) + adjust) / denom;
}

// Include success chance in targeter for spells checking monster MR.
vector<string> desc_success_chance(const monster_info& mi, int pow, bool evoked,
                                   targeter* hitfunc)
{
    targeter_beam* beam_hitf = dynamic_cast<targeter_beam*>(hitfunc);
    vector<string> descs;
    const int mr = mi.res_magic();
    if (mr == MAG_IMMUNE)
        descs.push_back("magic immune");
    else if (hitfunc && !hitfunc->affects_monster(mi))
        descs.push_back("not susceptible");
    // Polymorph has a special effect on ugly things and shapeshifters that
    // does not require passing an MR check.
    else if (beam_hitf && beam_hitf->beam.flavour == BEAM_POLYMORPH
             && (mi.type == MONS_UGLY_THING || mi.type == MONS_VERY_UGLY_THING
                 || mi.is(MB_SHAPESHIFTER)))
    {
        descs.push_back(make_stringf("will change %s",
                                     mi.is(MB_SHAPESHIFTER) ? "shape"
                                     /* ugly things */      : "colour"));
    }
    else
    {
#if TAG_MAJOR_VERSION == 34
        const int adj_pow = evoked ? pakellas_effective_hex_power(pow)
                                   : pow;
#else
        UNUSED(evoked);
        const int adj_pow = pow;
#endif
        const int success = hex_success_chance(mr, adj_pow, 100);
        descs.push_back(make_stringf("chance to defeat MR: %d%%", success));
    }
    return descs;
}

/**
 * Targets and fires player-cast spells & spell-like effects.
 *
 * Not all of these are actually real spells; invocations, decks or misc.
 * effects might also land us here.
 * Others are currently unused or unimplemented.
 *
 * @param spell         The type of spell being cast.
 * @param powc          Spellpower.
 * @param allow_fail    true if it is a spell being cast normally.
 *                      false if the spell is evoked or from an innate or divine ability
 *
 * @param evoked_item   The wand the spell was evoked from if applicable, or
                        nullptr.
 * @return spret::success if spell is successfully cast for purposes of
 * exercising, spret::fail otherwise, or spret::abort if the player cancelled
 * the casting.
 **/
spret your_spells(spell_type spell, int powc, bool allow_fail,
                       const item_def* const evoked_item)
{
    ASSERT(!crawl_state.game_is_arena());
    ASSERT(!evoked_item || evoked_item->base_type == OBJ_WANDS);

    const bool wiz_cast = (crawl_state.prev_cmd == CMD_WIZARD && !allow_fail);

    dist spd;
    bolt beam;
    beam.origin_spell = spell;

    // [dshaligram] Any action that depends on the spellcasting attempt to have
    // succeeded must be performed after the switch.
    if (!wiz_cast && _spellcasting_aborted(spell, !allow_fail))
        return spret::abort;

    const spell_flags flags = get_spell_flags(spell);

    ASSERT(wiz_cast || !(flags & spflag::testing));

    if (!powc)
        powc = calc_spell_power(spell, true);

    const int range = calc_spell_range(spell, powc, allow_fail);
    beam.range = range;

    // XXX: This handles only some of the cases where spells need
    // targeting. There are others that do their own that will be
    // missed by this (and thus will not properly ESC without cost
    // because of it). Hopefully, those will eventually be fixed. - bwr
    if (flags & spflag::targeting_mask)
    {
        const targ_mode_type targ =
              testbits(flags, spflag::neutral)    ? TARG_ANY :
              testbits(flags, spflag::helpful)    ? TARG_FRIEND :
              testbits(flags, spflag::obj)        ? TARG_MOVABLE_OBJECT :
                                                   TARG_HOSTILE;

        const targeting_type dir =
             testbits(flags, spflag::target) ? DIR_TARGET : DIR_NONE;

        const char *prompt = get_spell_target_prompt(spell);

        const bool needs_path = !testbits(flags, spflag::target)
                                // Apportation must be spflag::target, since a
                                // shift-direction makes no sense for it, but
                                // it nevertheless requires line-of-fire.
                                || spell == SPELL_APPORTATION;

        unique_ptr<targeter> hitfunc = _spell_targeter(spell, powc, range);

        // Add success chance to targeted spells checking monster MR
        const bool mr_check = testbits(flags, spflag::MR_check)
                              && testbits(flags, spflag::dir_or_target)
                              && !testbits(flags, spflag::helpful);
        desc_filter additional_desc = nullptr;
        if (mr_check)
        {
            const zap_type zap = spell_to_zap(spell);
            const int eff_pow = zap == NUM_ZAPS ? powc
                                                : zap_ench_power(zap, powc,
                                                                 false);
            additional_desc = bind(desc_success_chance, placeholders::_1,
                                   eff_pow, evoked_item, hitfunc.get());
        }

        string title = make_stringf("Aiming: <w>%s</w>", spell_title(spell));
        if (allow_fail)
        {
            title += make_stringf(" <lightgrey>(%s)</lightgrey>",
                _spell_failure_rate_description(spell).c_str());
        }

        direction_chooser_args args;
        args.hitfunc = hitfunc.get();
        args.restricts = dir;
        args.mode = targ;
        args.range = range;
        args.needs_path = needs_path;
        args.target_prefix = prompt;
        args.top_prompt = title;
        if (hitfunc && hitfunc->can_affect_walls())
        {
            args.show_floor_desc = true;
            args.show_boring_feats = false; // don't show "The floor."
        }
        if (testbits(flags, spflag::not_self))
            args.self = confirm_prompt_type::cancel;
        else
            args.self = confirm_prompt_type::none;
        args.get_desc_func = additional_desc;
        if (!spell_direction(spd, beam, &args))
            return spret::abort;

        if (testbits(flags, spflag::not_self) && spd.isMe())
        {
            if (spell == SPELL_TELEPORT_OTHER)
                mpr("Sorry, this spell works on others only.");
            else
                canned_msg(MSG_UNTHINKING_ACT);

            return spret::abort;
        }

        if (spd.isMe() && spell == SPELL_INVISIBILITY && !invis_allowed())
            return spret::abort;
    }

    if (evoked_item)
    {
#if TAG_MAJOR_VERSION == 34
        const int surge = pakellas_surge_devices();
#else
        const int surge = 0;
#endif
        powc = player_adjust_evoc_power(powc, surge);
#if TAG_MAJOR_VERSION == 34
        int mp_cost_of_wand = evoked_item->base_type == OBJ_WANDS
                              ? wand_mp_cost() : 0;
        surge_power_wand(mp_cost_of_wand + surge * 3);
#endif
    }
#if TAG_MAJOR_VERSION == 34
    else if (allow_fail)
        surge_power(_spell_enhancement(spell));
#endif
    // Enhancers only matter for calc_spell_power() and raw_spell_fail().
    // Not sure about this: is it flavour or misleading? (jpeg)

    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    int fail = 0;
#if TAG_MAJOR_VERSION == 34
    bool antimagic = false; // lost time but no other penalty

    if (allow_fail && you.duration[DUR_ANTIMAGIC]
        && x_chance_in_y(you.duration[DUR_ANTIMAGIC] / 3, you.hp_max))
    {
        mpr("You fail to access your magic.");
        fail = antimagic = true;
    }
    else
#endif
    if (evoked_item && evoked_item->charges == 0)
        return spret::fail;
    else if (allow_fail)
    {
        int spfl = random2avg(100, 3);

        if (!you_worship(GOD_SIF_MUNA)
            && you.penance[GOD_SIF_MUNA] && one_chance_in(20))
        {
            god_speaks(GOD_SIF_MUNA, "You feel a surge of divine spite.");

            // This will cause failure and increase the miscast effect.
            spfl = -you.penance[GOD_SIF_MUNA];
        }
        else if (spell_typematch(spell, spschool::necromancy)
                 && !you_worship(GOD_KIKUBAAQUDGHA)
                 && you.penance[GOD_KIKUBAAQUDGHA]
                 && one_chance_in(20))
        {
            // And you thought you'd Necromutate your way out of penance...
            simple_god_message(" does not allow the disloyal to dabble in "
                               "death!", GOD_KIKUBAAQUDGHA);

            // The spell still goes through, but you get a miscast anyway.
            miscast_effect(you, nullptr,
                           {miscast_source::god, GOD_KIKUBAAQUDGHA},
                           spschool::necromancy,
                           spell_difficulty(spell),
                           you.experience_level,
                           "the malice of Kikubaaqudgha");
        }
        else if (vehumet_supports_spell(spell)
                 && !you_worship(GOD_VEHUMET)
                 && you.penance[GOD_VEHUMET]
                 && one_chance_in(20))
        {
            // And you thought you'd Fire Storm your way out of penance...
            simple_god_message(" does not allow the disloyal to dabble in "
                               "destruction!", GOD_VEHUMET);

            // The spell still goes through, but you get a miscast anyway.
            miscast_effect(you, nullptr, {miscast_source::god, GOD_VEHUMET},
                           spschool::conjuration,
                           spell_difficulty(spell),
                           you.experience_level,
                           "the malice of Vehumet");
        }

        const int spfail_chance = raw_spell_fail(spell);

        if (spfl < spfail_chance)
            fail = spfail_chance - spfl;
    }

    dprf("Spell #%d, power=%d", spell, powc);

    // Have to set aim first, in case the spellcast kills its first target
    if (you.props.exists("battlesphere") && allow_fail)
        aim_battlesphere(&you, spell);

    const auto orig_target = monster_at(beam.target);
    const bool self_target = you.pos() == beam.target;
    const bool had_tele = orig_target && orig_target->has_ench(ENCH_TP);

    spret cast_result = _do_cast(spell, powc, spd, beam, god, fail);

    switch (cast_result)
    {
    case spret::success:
    {
        if (you.props.exists("battlesphere") && allow_fail)
            trigger_battlesphere(&you);

        const auto victim = monster_at(beam.target);
        if (will_have_passive(passive_t::shadow_spells)
            && allow_fail
            && !god_hates_spell(spell, you.religion, !allow_fail)
            && (flags & spflag::targeting_mask)
            && !(flags & spflag::neutral)
            && (beam.is_enchantment()
                || battlesphere_can_mirror(spell))
            // Must have a target, but that can't be the player.
            && !self_target
            && orig_target
            // For teleport other, only mimic if the spell hit who we
            // originally targeted and if we failed to change the target's
            // teleport status. This way the mimic won't just undo the effect
            // of a successful cast.
            && (spell != SPELL_TELEPORT_OTHER
                || (orig_target == victim
                    && had_tele == victim->has_ench(ENCH_TP))))
        {
            dithmenos_shadow_spell(&beam, spell);
        }
        _spellcasting_side_effects(spell, god, !allow_fail);
        return spret::success;
    }
    case spret::fail:
    {
#if TAG_MAJOR_VERSION == 34
        if (antimagic)
            return spret::fail;
#endif

        mprf("You miscast %s.", spell_title(spell));
        flush_input_buffer(FLUSH_ON_FAILURE);
        learned_something_new(HINT_SPELL_MISCAST);
        miscast_effect(spell, fail);

        return spret::fail;
    }

    case spret::abort:
        return spret::abort;

    case spret::none:
#ifdef WIZARD
        if (you.wizard && !allow_fail && is_valid_spell(spell)
            && (flags & spflag::monster))
        {
            _try_monster_cast(spell, powc, spd, beam);
            return spret::success;
        }
#endif

        if (is_valid_spell(spell))
        {
            mprf(MSGCH_ERROR, "Spell '%s' is not a player castable spell.",
                 spell_title(spell));
        }
        else
            mprf(MSGCH_ERROR, "Invalid spell!");

        return spret::abort;
    }

    return spret::success;
}

// Returns spret::success, spret::abort, spret::fail
// or spret::none (not a player spell).
static spret _do_cast(spell_type spell, int powc, const dist& spd,
                           bolt& beam, god_type god, bool fail)
{
    const coord_def target = spd.isTarget ? beam.target : you.pos() + spd.delta;
    if (spell == SPELL_FREEZE || spell == SPELL_VAMPIRIC_DRAINING)
    {
        if (!adjacent(you.pos(), target))
            return spret::abort;
    }

    switch (spell)
    {
    case SPELL_FREEZE:
        return cast_freeze(powc, monster_at(target), fail);

    case SPELL_SANDBLAST:
        return cast_sandblast(powc, beam, fail);

    case SPELL_VAMPIRIC_DRAINING:
        return vampiric_drain(powc, monster_at(target), fail);

    case SPELL_IOOD:
        return cast_iood(&you, powc, &beam, 0, 0, MHITNOT, fail);

    // Clouds and explosions.
    case SPELL_POISONOUS_CLOUD:
    case SPELL_HOLY_BREATH:
    case SPELL_FREEZING_CLOUD:
        return cast_big_c(powc, spell, &you, beam, fail);

    case SPELL_FIRE_STORM:
        return cast_fire_storm(powc, beam, fail);

    // Demonspawn ability, no failure.
    case SPELL_CALL_DOWN_DAMNATION:
        return cast_smitey_damnation(powc, beam) ? spret::success : spret::abort;

    // LOS spells

    // Beogh ability, no failure.
    case SPELL_SMITING:
        return cast_smiting(powc, monster_at(target)) ? spret::success
                                                      : spret::abort;

    case SPELL_AIRSTRIKE:
        return cast_airstrike(powc, spd, fail);

    case SPELL_LRD:
        return cast_fragmentation(powc, &you, spd.target, fail);

    case SPELL_GRAVITAS:
        return cast_gravitas(powc, beam.target, fail);

    // other effects
    case SPELL_DISCHARGE:
        return cast_discharge(powc, you, fail);

    case SPELL_CHAIN_LIGHTNING:
        return cast_chain_spell(SPELL_CHAIN_LIGHTNING, powc, &you, fail);

    case SPELL_DISPERSAL:
        return cast_dispersal(powc, fail);

    case SPELL_SHATTER:
        return cast_shatter(powc, fail);

    case SPELL_IRRADIATE:
        return cast_irradiate(powc, &you, fail);

    case SPELL_LEDAS_LIQUEFACTION:
        return cast_liquefaction(powc, fail);

    case SPELL_OZOCUBUS_REFRIGERATION:
        return fire_los_attack_spell(spell, powc, &you, fail);

    case SPELL_OLGREBS_TOXIC_RADIANCE:
        return cast_toxic_radiance(&you, powc, fail);

    case SPELL_IGNITE_POISON:
        return cast_ignite_poison(&you, powc, fail);

    case SPELL_TORNADO:
        return cast_tornado(powc, fail);

    case SPELL_THUNDERBOLT:
        return cast_thunderbolt(&you, powc, target, fail);

    case SPELL_DAZZLING_FLASH:
        return cast_dazzling_flash(powc, fail);

    case SPELL_CHAIN_OF_CHAOS:
        return cast_chain_spell(SPELL_CHAIN_OF_CHAOS, powc, &you, fail);

    case SPELL_IGNITION:
        return cast_ignition(&you, powc, fail);

    case SPELL_FROZEN_RAMPARTS:
        return cast_frozen_ramparts(powc, fail);

    // Summoning spells, and other spells that create new monsters.
    // If a god is making you cast one of these spells, any monsters
    // produced will count as god gifts.
    case SPELL_SUMMON_SMALL_MAMMAL:
        return cast_summon_small_mammal(powc, god, fail);

    case SPELL_CALL_CANINE_FAMILIAR:
        return cast_call_canine_familiar(powc, god, fail);

    case SPELL_SUMMON_ICE_BEAST:
        return cast_summon_ice_beast(powc, god, fail);

    case SPELL_MONSTROUS_MENAGERIE:
        return cast_monstrous_menagerie(&you, powc, god, fail);

    case SPELL_SUMMON_DRAGON:
        return cast_summon_dragon(&you, powc, god, fail);

    case SPELL_DRAGON_CALL:
        return cast_dragon_call(powc, fail);

    case SPELL_SUMMON_HYDRA:
        return cast_summon_hydra(&you, powc, god, fail);

    case SPELL_SUMMON_MANA_VIPER:
        return cast_summon_mana_viper(powc, god, fail);

    case SPELL_CONJURE_BALL_LIGHTNING:
        return cast_conjure_ball_lightning(powc, god, fail);

    case SPELL_SUMMON_LIGHTNING_SPIRE:
        return cast_summon_lightning_spire(powc, god, fail);

    case SPELL_SUMMON_GUARDIAN_GOLEM:
        return cast_summon_guardian_golem(powc, god, fail);

    case SPELL_CALL_IMP:
        return cast_call_imp(powc, god, fail);

    case SPELL_SUMMON_DEMON:
        return cast_summon_demon(powc, god, fail);

    case SPELL_SUMMON_GREATER_DEMON:
        return cast_summon_greater_demon(powc, god, fail);

    case SPELL_SHADOW_CREATURES:
        return cast_shadow_creatures(spell, god, level_id::current(), fail);

    case SPELL_SUMMON_HORRIBLE_THINGS:
        return cast_summon_horrible_things(powc, god, fail);

    case SPELL_MALIGN_GATEWAY:
        return cast_malign_gateway(&you, powc, god, fail);

    case SPELL_SUMMON_FOREST:
        return cast_summon_forest(&you, powc, god, fail);

    case SPELL_ANIMATE_SKELETON:
        return cast_animate_skeleton(powc, god, fail);

    case SPELL_ANIMATE_DEAD:
        return cast_animate_dead(powc, god, fail);

    case SPELL_SIMULACRUM:
        return cast_simulacrum(powc, god, fail);

    case SPELL_HAUNT:
        return cast_haunt(powc, beam.target, god, fail);

    case SPELL_DEATH_CHANNEL:
        return cast_death_channel(powc, god, fail);

    case SPELL_SPELLFORGED_SERVITOR:
        return cast_spellforged_servitor(powc, god, fail);

    case SPELL_BATTLESPHERE:
        return cast_battlesphere(&you, powc, god, fail);

    case SPELL_INFESTATION:
        return cast_infestation(powc, beam, fail);

    case SPELL_FOXFIRE:
        return cast_foxfire(powc, god, fail);

    case SPELL_NOXIOUS_BOG:
        return cast_noxious_bog(powc, fail);

    // Enchantments.
    case SPELL_CONFUSING_TOUCH:
        return cast_confusing_touch(powc, fail);

    case SPELL_CAUSE_FEAR:
        return mass_enchantment(ENCH_FEAR, powc, fail);

    case SPELL_INTOXICATE:
        return cast_intoxicate(powc, fail);

    case SPELL_DISCORD:
        return mass_enchantment(ENCH_INSANE, powc, fail);

    case SPELL_ENGLACIATION:
        return cast_englaciation(powc, fail);

    case SPELL_EXCRUCIATING_WOUNDS:
        return cast_excruciating_wounds(powc, fail);

    // Transformations.
    case SPELL_BEASTLY_APPENDAGE:
        return cast_transform(powc, transformation::appendage, fail);

    case SPELL_BLADE_HANDS:
        return cast_transform(powc, transformation::blade_hands, fail);

    case SPELL_SPIDER_FORM:
        return cast_transform(powc, transformation::spider, fail);

    case SPELL_STATUE_FORM:
        return cast_transform(powc, transformation::statue, fail);

    case SPELL_ICE_FORM:
        return cast_transform(powc, transformation::ice_beast, fail);

    case SPELL_HYDRA_FORM:
        return cast_transform(powc, transformation::hydra, fail);

    case SPELL_DRAGON_FORM:
        return cast_transform(powc, transformation::dragon, fail);

    case SPELL_NECROMUTATION:
        return cast_transform(powc, transformation::lich, fail);

    case SPELL_SWIFTNESS:
        return cast_swiftness(powc, fail);

    case SPELL_OZOCUBUS_ARMOUR:
        return ice_armour(powc, fail);

    case SPELL_SILENCE:
        return cast_silence(powc, fail);

    case SPELL_WEREBLOOD:
        return cast_wereblood(powc, fail);

    case SPELL_PORTAL_PROJECTILE:
        return cast_portal_projectile(powc, fail);

    // other
    case SPELL_BORGNJORS_REVIVIFICATION:
        return cast_revivification(powc, fail);

    case SPELL_SUBLIMATION_OF_BLOOD:
        return cast_sublimation_of_blood(powc, fail);

    case SPELL_DEATHS_DOOR:
        return cast_deaths_door(powc, fail);

    // Escape spells.
    case SPELL_BLINK:
        return cast_blink(fail);

    case SPELL_CONTROLLED_BLINK:
        return cast_controlled_blink(fail);

    case SPELL_CONJURE_FLAME:
        return conjure_flame(powc, fail);

    case SPELL_PASSWALL:
        return cast_passwall(beam.target, powc, fail);

    case SPELL_APPORTATION:
        return cast_apportation(powc, beam, fail);

    case SPELL_DISJUNCTION:
        return cast_disjunction(powc, fail);

    case SPELL_CORPSE_ROT:
        return cast_corpse_rot(fail);

    case SPELL_GOLUBRIAS_PASSAGE:
        return cast_golubrias_passage(beam.target, fail);

    case SPELL_FULMINANT_PRISM:
        return cast_fulminating_prism(&you, powc, beam.target, fail);

    case SPELL_SEARING_RAY:
        return cast_searing_ray(powc, beam, fail);

    case SPELL_GLACIATE:
        return cast_glaciate(&you, powc, target, fail);

    case SPELL_RANDOM_BOLT:
        return cast_random_bolt(powc, beam, fail);

    case SPELL_RANDOM_EFFECTS:
        return cast_random_effects(powc, beam, fail);

    case SPELL_POISONOUS_VAPOURS:
        return cast_poisonous_vapours(powc, spd, fail);

    case SPELL_STARBURST:
        return cast_starburst(powc, fail);

    case SPELL_HAILSTORM:
        return cast_hailstorm(powc, fail);

    case SPELL_ABSOLUTE_ZERO:
        return cast_absolute_zero(powc, fail);

    case SPELL_ISKENDERUNS_MYSTIC_BLAST:
        return cast_imb(powc, fail);

    // non-player spells that have a zap, but that shouldn't be called (e.g
    // because they will crash as a player zap).
    case SPELL_DRAIN_LIFE:
        return spret::none;

    default:
        if (spell_removed(spell))
        {
            mpr("Sorry, this spell is gone!");
            return spret::abort;
        }
        break;
    }

    // Finally, try zaps.
    zap_type zap = spell_to_zap(spell);
    if (zap != NUM_ZAPS)
    {
        return zapping(zap, spell_zap_power(spell, powc), beam, true, nullptr,
                       fail);
    }

    return spret::none;
}

// _tetrahedral_number: returns the nth tetrahedral number.
// This is the number of triples of nonnegative integers with sum < n.
// Called only by get_true_fail_rate.
static int _tetrahedral_number(int n)
{
    return n * (n+1) * (n+2) / 6;
}

// get_true_fail_rate: Takes the raw failure to-beat number
// and converts it to the actual chance of failure:
// the probability that random2avg(100,3) < raw_fail.
// Should probably use more constants, though I doubt the spell
// success algorithms will really change *that* much.
// Called only by failure_rate_to_int
static double _get_true_fail_rate(int raw_fail)
{
    // Need 3*random2avg(100,3) = random2(101) + random2(101) + random2(100)
    // to be (strictly) less than 3*raw_fail. Fun with tetrahedral numbers!

    // How many possible outcomes, considering all three dice?
    const int outcomes = 101 * 101 * 100;
    const int target = raw_fail * 3;

    if (target <= 100)
    {
        // The failures are exactly the triples of nonnegative integers
        // that sum to < target.
        return double(_tetrahedral_number(target)) / outcomes;
    }
    if (target <= 200)
    {
        // Some of the triples that sum to < target would have numbers
        // greater than 100, or a last number greater than 99, so aren't
        // possible outcomes. Apply the principle of inclusion-exclusion
        // by subtracting out these cases. The set of triples with first
        // number > 100 is isomorphic to the set of triples that sum to
        // 101 less; likewise for the second and third numbers (100 less
        // in the last case). Two or more out-of-range numbers would have
        // resulted in a sum of at least 201, so there is no overlap
        // among the three cases we are subtracting.
        return double(_tetrahedral_number(target)
                      - 2 * _tetrahedral_number(target - 101)
                      - _tetrahedral_number(target - 100)) / outcomes;
    }
    // The random2avg distribution is symmetric, so the last interval is
    // essentially the same as the first interval.
    return double(outcomes - _tetrahedral_number(300 - target)) / outcomes;
}

const double fail_hp_fraction[] =
{
    .10,
    .30,
    .50,
    .70,
};
/**
 * Compute the maximum miscast damage from the given spell
 *
 * The miscast code uses
 *     dam = div_rand_round(roll_dice(level, level + raw_fail), MISCAST_DIVISOR)
 */
int max_miscast_damage(spell_type spell)
{
    int raw_fail = raw_spell_fail(spell);
    int level = spell_difficulty(spell);

    // Impossible to get a damaging miscast
    if (level * level * raw_fail <= MISCAST_THRESHOLD)
        return 0;

    return div_round_up(level * (raw_fail + level), MISCAST_DIVISOR);
}


/**
 * Compute the tier of maximum severity of a miscast
 * @param spell     The spell to be checked.
 *
 * Tiers are defined by the relation between the maximum miscast damage
 * (given a miscast occurs):
 *
 * - safe, no chance of dangerous effect
 * - slightly dangerous, mdam <= 10% mhp
 * - dangerous, mdam <= 30% mhp
 * - quite dangerous, mdam <= 50% mhp
 * - extremely dangerous, mdam <= 70% mhp
 * - potentially lethal, higher mdam
 */
int fail_severity(spell_type spell)
{
    const int raw_fail = raw_spell_fail(spell);
    const int level = spell_difficulty(spell);

    // Impossible to get a damaging miscast
    if (level * level * raw_fail <= 150)
        return 0;

    const int max_damage = max_miscast_damage(spell);

    for (int i = 0; i < 4; ++i)
        if (max_damage <= fail_hp_fraction[i] * get_real_hp(true))
            return i + 1;

    return 5;
}

const char *fail_severity_adjs[] =
{
    "safe",
    "mildly dangerous",
    "dangerous",
    "quite dangerous",
    "extremely dangerous",
    "potentially lethal",
};
COMPILE_CHECK(ARRAYSZ(fail_severity_adjs) > 3);

// Chooses a colour for the failure rate display for a spell. The colour is
// based on the chance of getting a severity >= 2 miscast.
int failure_rate_colour(spell_type spell)
{
    const int severity = fail_severity(spell);
    return severity == 0 ? LIGHTGREY :
           severity == 1 ? WHITE :
           severity == 2 ? YELLOW :
           severity == 3 ? LIGHTRED :
           severity == 4 ? RED
                         : MAGENTA;
}

//Converts the raw failure rate into a number to be displayed.
int failure_rate_to_int(int fail)
{
    if (fail <= 0)
        return 0;
    else if (fail >= 100)
        return (fail + 100)/2;
    else
        return max(1, (int) (100 * _get_true_fail_rate(fail)));
}

/**
 * Convert the given failure rate into a percent, and return it as a string.
 *
 * @param fail      A raw failure rate (not a percent!)
 * @return          E.g. "79%".
 */
string failure_rate_to_string(int fail)
{
    return make_stringf("%d%%", failure_rate_to_int(fail));
}

string spell_failure_rate_string(spell_type spell)
{
    const string failure = failure_rate_to_string(raw_spell_fail(spell));
    const string colour = colour_to_str(failure_rate_colour(spell));
    return make_stringf("<%s>%s</%s>",
            colour.c_str(), failure.c_str(), colour.c_str());
}

static string _spell_failure_rate_description(spell_type spell)
{
    const string failure = failure_rate_to_string(raw_spell_fail(spell));
    const char *severity_adj = fail_severity_adjs[fail_severity(spell)];
    const string colour = colour_to_str(failure_rate_colour(spell));
    const char *col = colour.c_str();

    return make_stringf("<%s>%s</%s>; <%s>%s</%s> risk of failure",
            col, severity_adj, col, col, failure.c_str(), col);
}

string spell_noise_string(spell_type spell, int chop_wiz_display_width)
{
    const int casting_noise = spell_noise(spell);
    int effect_noise = spell_effect_noise(spell, false);
    zap_type zap = spell_to_zap(spell);
    if (effect_noise == 0 && zap != NUM_ZAPS)
    {
        bolt beem;
        zappy(zap, 0, false, beem);
        effect_noise = beem.loudness;
    }

    // A typical amount of noise.
    if (spell == SPELL_TORNADO)
        effect_noise = 15;

    const int noise = max(casting_noise, effect_noise);

    const char* noise_descriptions[] =
    {
        "Silent", "Almost silent", "Quiet", "A bit loud", "Loud", "Very loud",
        "Extremely loud", "Deafening"
    };

    const int breakpoints[] = { 1, 2, 4, 8, 15, 20, 30 };
    COMPILE_CHECK(ARRAYSZ(noise_descriptions) == 1 + ARRAYSZ(breakpoints));

    const char* desc = noise_descriptions[breakpoint_rank(noise, breakpoints,
                                                ARRAYSZ(breakpoints))];

#ifdef WIZARD
    if (you.wizard)
    {
        if (chop_wiz_display_width > 0)
        {
            ostringstream shortdesc;
            shortdesc << chop_string(desc, chop_wiz_display_width)
                      << "(" << to_string(noise) << ")";
            return shortdesc.str();
        }
        else
            return make_stringf("%s (%d)", desc, noise);
    }
    else
#endif
        return desc;
}

int power_to_barcount(int power)
{
    if (power == -1)
        return -1;

    const int breakpoints[] = { 10, 15, 25, 35, 50, 75, 100, 150, 200 };
    return breakpoint_rank(power, breakpoints, ARRAYSZ(breakpoints)) + 1;
}

static int _spell_power_bars(spell_type spell)
{
    const int cap = spell_power_cap(spell);
    if (cap == 0)
        return -1;
    const int power = min(calc_spell_power(spell, true, false, false), cap);
    return power_to_barcount(power);
}

#ifdef WIZARD
static string _wizard_spell_power_numeric_string(spell_type spell)
{
    const int cap = spell_power_cap(spell);
    if (cap == 0)
        return "N/A";
    const int power = min(calc_spell_power(spell, true, false, false), cap);
    return make_stringf("%d (%d)", power, cap);
}
#endif

string spell_power_string(spell_type spell)
{
#ifdef WIZARD
    if (you.wizard)
        return _wizard_spell_power_numeric_string(spell);
#endif

    const int numbars = _spell_power_bars(spell);
    const int capbars = power_to_barcount(spell_power_cap(spell));
    ASSERT(numbars <= capbars);
    if (numbars < 0)
        return "N/A";
    else
        return string(numbars, '#') + string(capbars - numbars, '.');
}

int calc_spell_range(spell_type spell, int power, bool allow_bonus)
{
    if (power == 0)
        power = calc_spell_power(spell, true, false, false);
    const int range = spell_range(spell, power, allow_bonus);

    return range;
}

/**
 * Give a string visually describing a given spell's range, as cast by the
 * player.
 *
 * @param spell     The spell in question.
 * @return          Something like "@-->.."
 */
string spell_range_string(spell_type spell)
{
    if (spell == SPELL_HAILSTORM)
        return "@.->"; // Special case: hailstorm is a ring

    const int cap      = spell_power_cap(spell);
    const int range    = calc_spell_range(spell, 0);
    const int maxrange = spell_range(spell, cap);

    return range_string(range, maxrange, '@');
}

/**
 * Give a string visually describing a given spell's range.
 *
 * E.g., for a spell of fixed range 1 (melee), "@>"
 *       for a spell of range 3, max range 5, "@-->.."
 *
 * @param range         The current range of the spell.
 * @param maxrange      The range the spell would have at max power.
 * @param caster_char   The character used to represent the caster.
 *                      Usually @ for the player.
 * @return              See above.
 */
string range_string(int range, int maxrange, char32_t caster_char)
{
    if (range <= 0)
        return "N/A";

    return stringize_glyph(caster_char) + string(range - 1, '-')
           + string(">") + string(maxrange - range, '.');
}

string spell_schools_string(spell_type spell)
{
    string desc;

    bool already = false;
    for (const auto bit : spschools_type::range())
    {
        if (spell_typematch(spell, bit))
        {
            if (already)
                desc += "/";
            desc += spelltype_long_name(bit);
            already = true;
        }
    }

    return desc;
}

void spell_skills(spell_type spell, set<skill_type> &skills)
{
    const spschools_type disciplines = get_spell_disciplines(spell);
    for (const auto bit : spschools_type::range())
        if (disciplines & bit)
            skills.insert(spell_type2skill(bit));
}

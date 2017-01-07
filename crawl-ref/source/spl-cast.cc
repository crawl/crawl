/**
 * @file
 * @brief Spell casting and miscast functions.
**/

#include "AppHdr.h"

#include "spl-cast.h"

#include <iomanip>
#include <sstream>

#include "areas.h"
#include "art-enum.h"
#include "beam.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "colour.h"
#include "database.h"
#include "describe.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "food.h"
#include "format.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::shadow_spells
#include "god-wrath.h"
#include "hints.h"
#include "item-prop.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-book.h"
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
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "transform.h"
#include "unicode.h"
#include "unwind.h"
#include "view.h"
#include "viewchar.h" // stringize_glyph

static int _spell_enhancement(spell_type spell);

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
    highlight = failure_rate_colour(spell);
    desc << "<" << colour_to_str(highlight) << ">";
    const string failure = failure_rate_to_string(raw_spell_fail(spell));
    desc << chop_string(failure, 12);
    desc << "</" << colour_to_str(highlight) << ">";
    desc << spell_difficulty(spell);

    return desc.str();
}

static string _spell_extra_description(spell_type spell, bool viewing)
{
    ostringstream desc;

    int highlight =  spell_highlight_by_utility(spell, COL_UNKNOWN, !viewing);

    desc << "<" << colour_to_str(highlight) << ">" << left;

    // spell name
    desc << chop_string(spell_title(spell), 30);

    // spell power, spell range, hunger level, noise
    const string rangestring = spell_range_string(spell);

    desc << chop_string(spell_power_string(spell), 13)
         << chop_string(rangestring, 9 + tagged_string_tag_length(rangestring))
         << chop_string(spell_hunger_string(spell), 8)
         << chop_string(spell_noise_string(spell, 10), 15);

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

#ifdef USE_TILE_LOCAL
    const bool text_only = false;
#else
    const bool text_only = true;
#endif

    ToggleableMenu spell_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                              | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING,
                              text_only);
    string titlestring = make_stringf("%-25.25s", title.c_str());
#ifdef USE_TILE_LOCAL
    {
        // [enne] - Hack. Make title an item so that it's aligned.
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(
                " " + titlestring + "         Type          "
                "                Failure  Level",
                " " + titlestring + "         Power        "
                "Range    " + "Hunger  " + "Noise          ",
                MEL_ITEM);
        me->colour = BLUE;
        spell_menu.add_entry(me);
    }
#else
    spell_menu.set_title(
        new ToggleableMenuEntry(
            " " + titlestring + "         Type          "
            "                Failure  Level",
            " " + titlestring + "         Power        "
            "Range    " + "Hunger  " + "Noise          ",
            MEL_TITLE));
#endif
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

#ifdef USE_TILE
        me->add_tile(tile_def(tileidx_spell(spell), TEX_GUI));
#endif
        spell_menu.add_entry(me);
    }

    while (true)
    {
        vector<MenuEntry*> sel = spell_menu.show();
        if (!crawl_state.doing_prev_cmd_again)
            redraw_screen();
        if (sel.empty())
            return 0;

        ASSERT(sel.size() == 1);
        ASSERT(sel[0]->hotkeys.size() == 1);
        if (spell_menu.menu_action == Menu::ACT_EXAMINE)
        {
            describe_spell(get_spell_by_letter(sel[0]->hotkeys[0]), nullptr);
            redraw_screen();
        }
        else
            return sel[0]->hotkeys[0];
    }
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

    if (you.duration[DUR_BRILLIANCE])
        fail_reduce = fail_reduce / 2;

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
    chance -= 6 * calc_spell_power(spell, false, true, false);
    chance -= (you.intel() * 2);

    const int armour_shield_penalty = player_armour_shield_spell_penalty();
    dprf("Armour+Shield spell failure penalty: %d", armour_shield_penalty);
    chance += armour_shield_penalty;

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
        330,
    };
    const int spell_level = spell_difficulty(spell);
    ASSERT_RANGE(spell_level, 0, (int) ARRAYSZ(difficulty_by_level));
    chance += difficulty_by_level[spell_level];

#if TAG_MAJOR_VERSION == 34
    // Only apply this penalty to Dj because other species lose nutrition
    // rather than gaining contamination when casting spells.
    // Also, this penalty gives fairly precise information about contam
    // level, and only Dj already has such information (on the contam bar).
    // Other species would have to check their failure rates all the time
    // when at yellow glow.
    if (you.species == SP_DJINNI)
    {
        int64_t contam = you.magic_contamination;
        // Just +25 on the edge of yellow glow, +200 in the middle of yellow,
        // forget casting when in orange.
        chance += contam * contam * contam / 5000000000LL;
    }
#endif

    int chance2 = chance;

    const int chance_breaks[][2] =
    {
        {45, 45}, {42, 43}, {38, 41}, {35, 40}, {32, 38}, {28, 36},
        {22, 34}, {16, 32}, {10, 30}, {2, 28}, {-7, 26}, {-12, 24},
        {-18, 22}, {-24, 20}, {-30, 18}, {-38, 16}, {-46, 14},
        {-60, 12}, {-80, 10}, {-100, 8}, {-120, 6}, {-140, 4},
        {-160, 2}, {-180, 0}
    };

    for (const int (&cbrk)[2] : chance_breaks)
        if (chance < cbrk[0])
            chance2 = cbrk[1];

    chance2 += get_form()->spellcasting_penalty;

    chance2 -= 2 * player_mutation_level(MUT_SUBDUED_MAGIC);
    chance2 += 4 * player_mutation_level(MUT_WILD_MAGIC);
    chance2 += 4 * player_mutation_level(MUT_ANTI_WIZARDRY);

    if (you.props.exists(SAP_MAGIC_KEY))
        chance2 += you.props[SAP_MAGIC_KEY].get_int() * 12;

    chance2 += you.duration[DUR_VERTIGO] ? 7 : 0;

    // Apply the effects of Vehumet and items of wizardry.
    chance2 = _apply_spellcasting_success_boosts(spell, chance2);

    if (chance2 > 100)
        chance2 = 100;

    return chance2;
}

int stepdown_spellpower(int power)
{
    return stepdown_value(power / 100, 50, 50, 150, 200);
}

int calc_spell_power(spell_type spell, bool apply_intel, bool fail_rate_check,
                     bool cap_power)
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

    // Brilliance boosts spell power a bit (equivalent to three
    // spell school levels).
    if (!fail_rate_check && you.duration[DUR_BRILLIANCE])
        power += 600;

    if (apply_intel)
        power = (power * you.intel()) / 10;

    // [dshaligram] Enhancers don't affect fail rates any more, only spell
    // power. Note that this does not affect Vehumet's boost in castability.
    if (!fail_rate_check)
        power = apply_enhancement(power, _spell_enhancement(spell));

    // Wild magic boosts spell power but decreases success rate.
    if (!fail_rate_check)
    {
        power *= (10 + 3 * player_mutation_level(MUT_WILD_MAGIC));
        power /= (10 + 3 * player_mutation_level(MUT_SUBDUED_MAGIC));
    }

    // Augmentation boosts spell power at high HP.
    if (!fail_rate_check)
    {
        power *= 10 + 4 * augmentation_amount();
        power /= 10;
    }

    // Each level of horror reduces spellpower by 10%
    if (you.duration[DUR_HORROR] && !fail_rate_check)
    {
        power *= 10;
        power /= 10 + (you.props[HORROR_PENALTY_KEY].get_int() * 3) / 2;
    }

    power = stepdown_spellpower(power);

    const int cap = spell_power_cap(spell);
    if (cap > 0 && cap_power)
        power = min(power, cap);

    return power;
}

static int _spell_enhancement(spell_type spell)
{
    const spschools_type typeflags = get_spell_disciplines(spell);
    int enhanced = 0;

    if (typeflags & SPTYP_CONJURATION)
        enhanced += player_spec_conj();

    if (typeflags & SPTYP_HEXES)
        enhanced += player_spec_hex();

    if (typeflags & SPTYP_CHARMS)
        enhanced += player_spec_charm();

    if (typeflags & SPTYP_SUMMONING)
        enhanced += player_spec_summ();

    if (typeflags & SPTYP_POISON)
        enhanced += player_spec_poison();

    if (typeflags & SPTYP_NECROMANCY)
        enhanced += player_spec_death();

    if (typeflags & SPTYP_FIRE)
        enhanced += player_spec_fire();

    if (typeflags & SPTYP_ICE)
        enhanced += player_spec_cold();

    if (typeflags & SPTYP_EARTH)
        enhanced += player_spec_earth();

    if (typeflags & SPTYP_AIR)
        enhanced += player_spec_air();

    if (you.form == TRAN_SHADOW)
        enhanced -= 2;

    enhanced += you.archmagi();
    enhanced += player_equip_unrand(UNRAND_MAJIN);

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_LAVA_BOOST)
        && (typeflags & SPTYP_FIRE) && (typeflags & SPTYP_EARTH))
    {
        enhanced++;
    }
#endif

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

bool can_cast_spells(bool quiet)
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

    if (!you.spell_no)
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

    if (you.duration[DUR_NO_CAST])
    {
        if (!quiet)
            mpr("You are unable to access your magic!");
        return false;
    }

    if (!you.undead_state() && !you_foodless()
        && you.hunger_state <= HS_STARVING)
    {
        if (!quiet)
            canned_msg(MSG_NO_ENERGY);
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
    if (!can_cast_spells())
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    if (crawl_state.game_is_hints())
        Hints.hints_spell_counter++;

    if (spell == SPELL_NO_SPELL)
    {
        int keyin = 0;

        while (true)
        {
#ifdef TOUCH_UI
            keyin = list_spells(true, false);
            if (!keyin)
                keyin = ESCAPE;

            if (!crawl_state.doing_prev_cmd_again)
                redraw_screen();

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

                if (you.last_cast_spell == SPELL_NO_SPELL
                    || !Options.enable_recast_spell)
                {
                    mprf(MSGCH_PROMPT, "Cast which spell? (? or * to list) ");
                }
                else
                {
                    mprf(MSGCH_PROMPT, "Casting: <w>%s</w>",
                         spell_title(you.last_cast_spell));
                    mprf(MSGCH_PROMPT, "Confirm with . or Enter, or press ? or * to list all spells.");
                }

                keyin = get_ch();
            }

            if (keyin == '?' || keyin == '*')
            {
                keyin = list_spells(true, false);
                if (!keyin)
                    keyin = ESCAPE;

                if (!crawl_state.doing_prev_cmd_again)
                    redraw_screen();

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
    int sifcast_amount = 0;
    if (!enough_mp(cost, true))
    {
        if (you.attribute[ATTR_DIVINE_ENERGY])
        {
            sifcast_amount = cost - you.magic_points;
            cost = you.magic_points;
        }
        else
        {
            mpr("You don't have enough magic to cast that spell.");
            crawl_state.zero_turns_taken();
            return false;
        }
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

    if (you.undead_state() == US_ALIVE && !you_foodless()
        && you.hunger <= spell_hunger(spell))
    {
        canned_msg(MSG_NO_ENERGY);
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
            "<lightred>Casting this spell will cause instant excommunication!"
                "</lightred> Really cast?" :
            "Casting this spell will place you under penance. Really cast?",
            true, 'n'))
        {
            canned_msg(MSG_OK);
            crawl_state.zero_turns_taken();
            return false;
        }
    }

    const bool staff_energy = player_energy();
    you.last_cast_spell = spell;
    // Silently take MP before the spell.
    dec_mp(cost, true);

    const spret_type cast_result = your_spells(spell, 0, true);
    if (cast_result == SPRET_ABORT)
    {
        crawl_state.zero_turns_taken();
        // Return the MP since the spell is aborted.
        inc_mp(cost, true);
        return false;
    }

    practise_casting(spell, cast_result == SPRET_SUCCESS);
    if (cast_result == SPRET_SUCCESS)
    {
        did_god_conduct(DID_SPELL_CASTING, 1 + random2(5));
        count_action(CACT_CAST, spell);
    }

#if TAG_MAJOR_VERSION == 34
    // Nasty special cases.
    if (you.species == SP_DJINNI && cast_result == SPRET_SUCCESS
        && (spell == SPELL_BORGNJORS_REVIVIFICATION
         || spell == SPELL_SUBLIMATION_OF_BLOOD && you.hp == you.hp_max))
    {
        // These spells have replenished essence to full.
        inc_mp(cost, true);
    }
    else // Redraw MP
#endif
        flush_mp();

    if (!staff_energy && you.undead_state() != US_UNDEAD)
    {
        const int spellh = spell_hunger(spell);
        if (calc_hunger(spellh) > 0)
        {
            make_hungry(spellh, true, true);
            learned_something_new(HINT_SPELL_HUNGER);
        }
    }

    if (sifcast_amount)
    {
        simple_god_message(" grants you divine energy.");
        mpr("You briefly lose access to your magic!");
        you.set_duration(DUR_NO_CAST, 3 + random2avg(sifcast_amount * 2, 2));
    }

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

    if (is_corpse_violating_spell(spell))
        did_god_conduct(DID_CORPSE_VIOLATION, conduct_level);

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
 * @param fake_spell    Whether the spell is evoked, or from an innate or
 *                      divine ability.
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
static void _try_monster_cast(spell_type spell, int powc,
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

static void _maybe_cancel_repeat(spell_type spell)
{
    switch (spell)
    {
    case SPELL_DELAYED_FIREBALL:        crawl_state.cant_cmd_repeat(make_stringf("You can't repeat %s.",
                                                 spell_title(spell)));
        break;

    default:
        break;
    }
}

static spret_type _do_cast(spell_type spell, int powc, const dist& spd,
                           bolt& beam, god_type god, bool fail);

/**
 * Should this spell be aborted before casting properly starts, either because
 * it can't legally be cast in this circumstance, or because the player opts
 * to cancel it in response to a prompt?
 *
 * @param spell         The spell to be checked.
 * @param fake_spell    Whether the spell is evoked, or from an innate or
 *                      divine ability.
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

    if (msg != "")
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
    if (Options.fail_severity_to_confirm > 0
        && Options.fail_severity_to_confirm <= severity
        && !crawl_state.disables[DIS_CONFIRMATIONS]
        && !fake_spell)
    {
        string prompt = make_stringf("The spell is %s to cast%s "
                                     "Continue anyway?",
                                     fail_severity_adjs[severity],
                                     severity > 1 ? "!" : ".");

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
    case SPELL_ISKENDERUNS_MYSTIC_BLAST:
        return make_unique<targeter_imb>(&you, pow, range);
    case SPELL_FIRE_STORM:
        return make_unique<targeter_smite>(&you, range, 2, pow > 76 ? 3 : 2);
    case SPELL_FREEZING_CLOUD:
    case SPELL_POISONOUS_CLOUD:
    case SPELL_HOLY_BREATH:
        return make_unique<targeter_cloud>(&you, range);
    case SPELL_THUNDERBOLT:
        return make_unique<targeter_thunderbolt>(&you, range,
            (you.props.exists("thunderbolt_last")
             && you.props["thunderbolt_last"].get_int() + 1 == you.num_turns) ?
                you.props["thunderbolt_aim"].get_coord() : coord_def());
    case SPELL_LRD:
        return make_unique<targeter_fragment>(&you, pow, range);
    case SPELL_FULMINANT_PRISM:
        return make_unique<targeter_smite>(&you, range, 0, 2);
    case SPELL_DAZZLING_SPRAY:
        return make_unique<targeter_spray>(&you, range, ZAP_DAZZLING_SPRAY);
    case SPELL_GLACIATE:
        return make_unique<targeter_cone>(&you, range);
    case SPELL_CLOUD_CONE:
        return make_unique<targeter_shotgun>(&you, CLOUD_CONE_BEAM_COUNT,
                                             range);
    case SPELL_SCATTERSHOT:
        return make_unique<targeter_shotgun>(&you, shotgun_beam_count(pow),
                                             range);
    case SPELL_GRAVITAS:
        return make_unique<targeter_smite>(&you, range,
                                           gravitas_range(pow, 2),
                                           gravitas_range(pow));
    case SPELL_VIOLENT_UNRAVELLING:
        return make_unique<targeter_unravelling>(&you, range, pow);
    case SPELL_RANDOM_BOLT:
        return make_unique<targeter_beam>(&you, range, ZAP_CRYSTAL_BOLT, pow,
                                          0, 0);
    case SPELL_INFESTATION:
        return make_unique<targeter_smite>(&you, range, 2, 2, false,
                                           [](const coord_def& p) -> bool {
                                              return you.pos() != p; });

    default:
        break;
    }

    if (spell_to_zap(spell) != NUM_ZAPS)
        return make_unique<targeter_beam>(&you, range, spell_to_zap(spell),
                                          pow, 0, 0);

    return nullptr;
}

static double _chance_miscast_prot()
{
    double miscast_prot = 0;

    if (have_passive(passive_t::miscast_protection))
        miscast_prot = (double) you.piety/piety_breakpoint(5);

    return min(1.0, miscast_prot);
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
    vector<string> descs;
    const int mr = mi.res_magic();
    if (mr == MAG_IMMUNE)
        descs.push_back("magic immune");
    else if (hitfunc && !hitfunc->affects_monster(mi))
        descs.push_back("not susceptible");
    else
    {
        const int adj_pow = evoked ? pakellas_effective_hex_power(pow)
                                   : pow;
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
 * @param allow_fail    Whether spell-fail chance applies.
 * @param evoked_item   The wand the spell was evoked from if applicable, or
                        nullptr.
 * @return SPRET_SUCCESS if spell is successfully cast for purposes of
 * exercising, SPRET_FAIL otherwise, or SPRET_ABORT if the player cancelled
 * the casting.
 **/
spret_type your_spells(spell_type spell, int powc, bool allow_fail,
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
        return SPRET_ABORT;

    const unsigned int flags = get_spell_flags(spell);

    ASSERT(wiz_cast || !(flags & SPFLAG_TESTING));

    if (!powc)
        powc = calc_spell_power(spell, true);

    // XXX: This handles only some of the cases where spells need
    // targeting. There are others that do their own that will be
    // missed by this (and thus will not properly ESC without cost
    // because of it). Hopefully, those will eventually be fixed. - bwr
    if (flags & SPFLAG_TARGETING_MASK)
    {
        const targ_mode_type targ =
              testbits(flags, SPFLAG_NEUTRAL)    ? TARG_ANY :
              testbits(flags, SPFLAG_HELPFUL)    ? TARG_FRIEND :
              testbits(flags, SPFLAG_OBJ)        ? TARG_MOVABLE_OBJECT :
                                                   TARG_HOSTILE;

        const targeting_type dir =
             testbits(flags, SPFLAG_TARGET) ? DIR_TARGET :
             testbits(flags, SPFLAG_DIR)    ? DIR_DIR    :
                                              DIR_NONE;

        const char *prompt = get_spell_target_prompt(spell);
        if (dir == DIR_DIR)
            mprf(MSGCH_PROMPT, "%s", prompt ? prompt : "Which direction?");

        const bool needs_path = !testbits(flags, SPFLAG_TARGET)
                                // Apportation must be SPFLAG_TARGET, since a
                                // shift-direction makes no sense for it, but
                                // it nevertheless requires line-of-fire.
                                || spell == SPELL_APPORTATION;

        const int range = calc_spell_range(spell, powc);

        unique_ptr<targeter> hitfunc = _spell_targeter(spell, powc, range);

        // Add success chance to targeted spells checking monster MR
        const bool mr_check = testbits(flags, SPFLAG_MR_CHECK)
                              && testbits(flags, SPFLAG_DIR_OR_TARGET)
                              && !testbits(flags, SPFLAG_HELPFUL);
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

        string title = "Aiming: <white>";
        title += spell_title(spell);
        title += "</white>";
        if (evoked_item
            && !item_ident(*evoked_item, ISFLAG_KNOW_PLUSES))
        {
            title += " <lightred>(will waste charges)</lightred>";
        }

        direction_chooser_args args;
        args.hitfunc = hitfunc.get();
        args.restricts = dir;
        args.mode = targ;
        args.range = range;
        args.needs_path = needs_path;
        args.target_prefix = prompt;
        args.top_prompt = title;
        if (testbits(flags, SPFLAG_NOT_SELF))
            args.self = CONFIRM_CANCEL;
        else
            args.self = CONFIRM_NONE;
        args.get_desc_func = additional_desc;
        if (!spell_direction(spd, beam, &args))
            return SPRET_ABORT;

        beam.range = range;

        if (testbits(flags, SPFLAG_NOT_SELF) && spd.isMe())
        {
            if (spell == SPELL_TELEPORT_OTHER)
                mpr("Sorry, this spell works on others only.");
            else
                canned_msg(MSG_UNTHINKING_ACT);

            return SPRET_ABORT;
        }

        if (spd.isMe() && spell == SPELL_INVISIBILITY && !invis_allowed())
            return SPRET_ABORT;
    }

    if (evoked_item)
    {
        const int surge = pakellas_surge_devices();
        powc = player_adjust_evoc_power(powc, surge);
        surge_power(you.spec_evoke() + surge);
    }
    else if (allow_fail)
        surge_power(_spell_enhancement(spell));
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
        return SPRET_FAIL;
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
        else if (spell_typematch(spell, SPTYP_NECROMANCY)
                 && !you_worship(GOD_KIKUBAAQUDGHA)
                 && you.penance[GOD_KIKUBAAQUDGHA]
                 && one_chance_in(20))
        {
            // And you thought you'd Necromutate your way out of penance...
            simple_god_message(" does not allow the disloyal to dabble in "
                               "death!", GOD_KIKUBAAQUDGHA);

            // The spell still goes through, but you get a miscast anyway.
            MiscastEffect(&you, nullptr, GOD_MISCAST + GOD_KIKUBAAQUDGHA,
                          SPTYP_NECROMANCY,
                          (you.experience_level / 2) + (spell_difficulty(spell) * 2),
                          random2avg(88, 3), "the malice of Kikubaaqudgha");
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
            MiscastEffect(&you, nullptr, GOD_MISCAST + GOD_VEHUMET,
                          SPTYP_CONJURATION,
                          (you.experience_level / 2) + (spell_difficulty(spell) * 2),
                          random2avg(88, 3), "the malice of Vehumet");
        }

        const int spfail_chance = raw_spell_fail(spell);

        if (spfl < spfail_chance)
            fail = spfail_chance - spfl;
    }

    dprf("Spell #%d, power=%d", spell, powc);

    if (crawl_state.prev_cmd == CMD_CAST_SPELL && god == GOD_NO_GOD)
        _maybe_cancel_repeat(spell);

    // Have to set aim first, in case the spellcast kills its first target
    if (you.props.exists("battlesphere") && allow_fail)
        aim_battlesphere(&you, spell, powc, beam);

    const bool old_target = actor_at(beam.target);

    spret_type cast_result = _do_cast(spell, powc, spd, beam, god, fail);

    switch (cast_result)
    {
    case SPRET_SUCCESS:
    {
        if (you.props.exists("battlesphere") && allow_fail)
            trigger_battlesphere(&you, beam);
        actor* victim = actor_at(beam.target);
        if (will_have_passive(passive_t::shadow_spells)
            && allow_fail
            && !god_hates_spell(spell, you.religion, !allow_fail)
            && (flags & SPFLAG_TARGETING_MASK)
            && !(flags & SPFLAG_NEUTRAL)
            && (beam.is_enchantment()
                || battlesphere_can_mirror(spell))
            && (!old_target || (victim && !victim->is_player())))
        {
            dithmenos_shadow_spell(&beam, spell);
        }
        _spellcasting_side_effects(spell, god, !allow_fail);
        return SPRET_SUCCESS;
    }
    case SPRET_FAIL:
    {
#if TAG_MAJOR_VERSION == 34
        if (antimagic)
            return SPRET_FAIL;
#endif

        mprf("You miscast %s.", spell_title(spell));
        flush_input_buffer(FLUSH_ON_FAILURE);
        learned_something_new(HINT_SPELL_MISCAST);

        if (decimal_chance(_chance_miscast_prot()))
        {
            simple_god_message(" protects you from the effects of your miscast!");
            return SPRET_FAIL;
        }

        // All spell failures give a bit of magical radiation.
        // Failure is a function of power squared multiplied by how
        // badly you missed the spell. High power spells can be
        // quite nasty: 9 * 9 * 90 / 500 = 15 points of
        // contamination!
        int nastiness = spell_difficulty(spell) * spell_difficulty(spell)
                        * fail + 250;

        const int cont_points = 2 * nastiness;

        // miscasts are uncontrolled
        contaminate_player(cont_points, true);

        MiscastEffect(&you, nullptr, SPELL_MISCAST, spell,
                      spell_difficulty(spell), fail);

        return SPRET_FAIL;
    }

    case SPRET_ABORT:
        return SPRET_ABORT;

    case SPRET_NONE:
#ifdef WIZARD
        if (you.wizard && !allow_fail && is_valid_spell(spell)
            && (flags & SPFLAG_MONSTER))
        {
            _try_monster_cast(spell, powc, spd, beam);
            return SPRET_SUCCESS;
        }
#endif

        if (is_valid_spell(spell))
        {
            mprf(MSGCH_ERROR, "Spell '%s' is not a player castable spell.",
                 spell_title(spell));
        }
        else
            mprf(MSGCH_ERROR, "Invalid spell!");

        return SPRET_ABORT;
    }

    return SPRET_SUCCESS;
}

// Returns SPRET_SUCCESS, SPRET_ABORT, SPRET_FAIL
// or SPRET_NONE (not a player spell).
static spret_type _do_cast(spell_type spell, int powc, const dist& spd,
                           bolt& beam, god_type god, bool fail)
{
    const coord_def target = spd.isTarget ? beam.target : you.pos() + spd.delta;
    if (spell == SPELL_FREEZE || spell == SPELL_VAMPIRIC_DRAINING)
    {
        if (!adjacent(you.pos(), target))
            return SPRET_ABORT;
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
        return cast_smitey_damnation(powc, beam) ? SPRET_SUCCESS : SPRET_ABORT;

    case SPELL_DELAYED_FIREBALL:
        return cast_delayed_fireball(fail);

    // LOS spells

    // Beogh ability, no failure.
    case SPELL_SMITING:
        return cast_smiting(powc, monster_at(target)) ? SPRET_SUCCESS
                                                      : SPRET_ABORT;

    case SPELL_AIRSTRIKE:
        return cast_airstrike(powc, spd, fail);

    case SPELL_LRD:
        return cast_fragmentation(powc, &you, spd.target, fail);

    case SPELL_GRAVITAS:
        return cast_gravitas(powc, beam.target, fail);

    // other effects
    case SPELL_DISCHARGE:
        return cast_discharge(powc, fail);

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

    case SPELL_DAZZLING_SPRAY:
        return cast_dazzling_spray(powc, target, fail);

    case SPELL_CHAIN_OF_CHAOS:
        return cast_chain_spell(SPELL_CHAIN_OF_CHAOS, powc, &you, fail);

    case SPELL_CLOUD_CONE:
        return cast_cloud_cone(&you, powc, target, fail);

    // Summoning spells, and other spells that create new monsters.
    // If a god is making you cast one of these spells, any monsters
    // produced will count as god gifts.
    case SPELL_SUMMON_BUTTERFLIES:
        return cast_summon_butterflies(powc, god, fail);

    case SPELL_SUMMON_SMALL_MAMMAL:
        return cast_summon_small_mammal(powc, god, fail);

    case SPELL_STICKS_TO_SNAKES:
        return cast_sticks_to_snakes(powc, god, fail);

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
        return cast_summon_lightning_spire(powc, beam.target, god, fail);

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
        return cast_animate_skeleton(god, fail);

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

    case SPELL_SPECTRAL_WEAPON:
        return cast_spectral_weapon(&you, powc, god, fail);

    case SPELL_BATTLESPHERE:
        return cast_battlesphere(&you, powc, god, fail);

    case SPELL_INFESTATION:
        return cast_infestation(powc, beam, fail);

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

    case SPELL_CONTROL_UNDEAD:
        return mass_enchantment(ENCH_CHARM, powc, fail);

    case SPELL_AURA_OF_ABJURATION:
        return cast_aura_of_abjuration(powc, fail);

    case SPELL_EXCRUCIATING_WOUNDS:
        return cast_excruciating_wounds(powc, fail);

    // Transformations.
    case SPELL_BEASTLY_APPENDAGE:
        return cast_transform(powc, TRAN_APPENDAGE, fail);

    case SPELL_BLADE_HANDS:
        return cast_transform(powc, TRAN_BLADE_HANDS, fail);

    case SPELL_SPIDER_FORM:
        return cast_transform(powc, TRAN_SPIDER, fail);

    case SPELL_STATUE_FORM:
        return cast_transform(powc, TRAN_STATUE, fail);

    case SPELL_ICE_FORM:
        return cast_transform(powc, TRAN_ICE_BEAST, fail);

    case SPELL_HYDRA_FORM:
        return cast_transform(powc, TRAN_HYDRA, fail);

    case SPELL_DRAGON_FORM:
        return cast_transform(powc, TRAN_DRAGON, fail);

    case SPELL_NECROMUTATION:
        return cast_transform(powc, TRAN_LICH, fail);

    // General enhancement.
    case SPELL_REGENERATION:
        return cast_regen(powc, fail);

    case SPELL_REPEL_MISSILES:
        return missile_prot(powc, fail);

    case SPELL_DEFLECT_MISSILES:
        return deflection(powc, fail);

    case SPELL_SWIFTNESS:
        return cast_swiftness(powc, fail);

    case SPELL_OZOCUBUS_ARMOUR:
        return ice_armour(powc, fail);

    case SPELL_CIGOTUVIS_EMBRACE:
        return corpse_armour(powc, fail);

    case SPELL_SILENCE:
        return cast_silence(powc, fail);

    case SPELL_INFUSION:
        return cast_infusion(powc, fail);

    case SPELL_SONG_OF_SLAYING:
        return cast_song_of_slaying(powc, fail);

    case SPELL_PORTAL_PROJECTILE:
        return cast_portal_projectile(powc, fail);

    // other
    case SPELL_BORGNJORS_REVIVIFICATION:
        return cast_revivification(powc, fail);

    case SPELL_SUBLIMATION_OF_BLOOD:
        return cast_sublimation_of_blood(powc, fail);

    case SPELL_DEATHS_DOOR:
        return cast_deaths_door(powc, fail);

    case SPELL_RING_OF_FLAMES:
        return cast_ring_of_flames(powc, fail);

    // Escape spells.
    case SPELL_BLINK:
        return cast_blink(fail);

    case SPELL_CONTROLLED_BLINK:
        return cast_controlled_blink(fail);

    case SPELL_CONJURE_FLAME:
        return conjure_flame(&you, powc, beam.target, fail);

    case SPELL_PASSWALL:
        return cast_passwall(spd.delta, powc, fail);

    case SPELL_APPORTATION:
        return cast_apportation(powc, beam, fail);

    case SPELL_RECALL:
        return cast_recall(fail);

    case SPELL_DISJUNCTION:
        return cast_disjunction(powc, fail);

    case SPELL_CORPSE_ROT:
        return cast_corpse_rot(fail);

    case SPELL_GOLUBRIAS_PASSAGE:
        return cast_golubrias_passage(beam.target, fail);

    case SPELL_DARKNESS:
        return cast_darkness(powc, fail);

    case SPELL_SHROUD_OF_GOLUBRIA:
        return cast_shroud_of_golubria(powc, fail);

    case SPELL_FULMINANT_PRISM:
        return cast_fulminating_prism(&you, powc, beam.target, fail);

    case SPELL_SEARING_RAY:
        return cast_searing_ray(powc, beam, fail);

    case SPELL_GLACIATE:
        return cast_glaciate(&you, powc, target, fail);

    case SPELL_RANDOM_BOLT:
        return cast_random_bolt(powc, beam, fail);

    case SPELL_SCATTERSHOT:
        return cast_scattershot(&you, powc, target, fail);

    case SPELL_RANDOM_EFFECTS:
        return cast_random_effects(powc, beam, fail);

    case SPELL_POISONOUS_VAPOURS:
        return cast_poisonous_vapours(powc, spd, fail);

    default:
        if (spell_removed(spell))
        {
            mpr("Sorry, this spell is gone!");
            return SPRET_ABORT;
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

    return SPRET_NONE;
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
// Called only by failure_rate_to_int and get_miscast_chance.
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

/**
 * Compute the chance of getting a miscast effect of a given severity or higher.
 * @param spell     The spell to be checked.
 * @param severity  Check the chance of getting a miscast this severe or higher.
 * @return          The chance of this kind of miscast.
 */
double get_miscast_chance(spell_type spell, int severity)
{
    int raw_fail = raw_spell_fail(spell);
    int level = spell_difficulty(spell);
    if (severity <= 0)
        return _get_true_fail_rate(raw_fail);
    double C = 70000.0 / (150 * level * (10 + level));
    double chance = 0.0;
    int k = severity + 1;
    while ((C * k) <= raw_fail)
    {
        chance += _get_true_fail_rate((int) (raw_fail + 1 - (C * k)))
            * severity / (k * (k - 1));
        k++;
    }
    return chance;
}

static double _get_miscast_chance_with_miscast_prot(spell_type spell)
{
    double raw_chance = get_miscast_chance(spell);
    double miscast_prot = _chance_miscast_prot();
    double chance = raw_chance * (1 - miscast_prot);

    return chance;
}

const char *fail_severity_adjs[] =
{
    "safe",
    "slightly dangerous",
    "quite dangerous",
    "very dangerous",
};
COMPILE_CHECK(ARRAYSZ(fail_severity_adjs) > 3);

int fail_severity(spell_type spell)
{
    const double chance = _get_miscast_chance_with_miscast_prot(spell);

    return (chance < 0.001) ? 0 :
           (chance < 0.005) ? 1 :
           (chance < 0.025) ? 2
                            : 3;
}

// Chooses a colour for the failure rate display for a spell. The colour is
// based on the chance of getting a severity >= 2 miscast.
int failure_rate_colour(spell_type spell)
{
    const int severity = fail_severity(spell);
    return severity == 0 ? LIGHTGREY :
           severity == 1 ? YELLOW :
           severity == 2 ? LIGHTRED
                         : RED;
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

string spell_hunger_string(spell_type spell)
{
    return hunger_cost_string(spell_hunger(spell));
}

string spell_noise_string(spell_type spell, int chop_wiz_display_width)
{
    const int casting_noise = spell_noise(spell);
    int effect_noise = spell_effect_noise(spell);
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

int calc_spell_range(spell_type spell, int power)
{
    if (power == 0)
        power = calc_spell_power(spell, true, false, false);
    const int range = spell_range(spell, power);

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

const set<spell_type> removed_spells =
{
#if TAG_MAJOR_VERSION == 34
    SPELL_ABJURATION,
    SPELL_CIGOTUVIS_DEGENERATION,
    SPELL_CONDENSATION_SHIELD,
    SPELL_CONTROL_TELEPORT,
    SPELL_DEMONIC_HORDE,
    SPELL_EVAPORATE,
    SPELL_FIRE_BRAND,
    SPELL_FORCEFUL_DISMISSAL,
    SPELL_FREEZING_AURA,
    SPELL_FULSOME_DISTILLATION,
    SPELL_INSULATION,
    SPELL_LETHAL_INFUSION,
    SPELL_POISON_WEAPON,
    SPELL_SEE_INVISIBLE,
    SPELL_SINGULARITY,
    SPELL_SONG_OF_SHIELDING,
    SPELL_SUMMON_SCORPIONS,
    SPELL_SUMMON_ELEMENTAL,
    SPELL_TWISTED_RESURRECTION,
    SPELL_SURE_BLADE,
    SPELL_FLY,
    SPELL_STONESKIN,
    SPELL_SUMMON_SWARM,
    SPELL_PHASE_SHIFT,
    SPELL_MASS_CONFUSION,
    SPELL_CURE_POISON,
#endif
};

bool spell_removed(spell_type spell)
{
    return removed_spells.count(spell) != 0;
}

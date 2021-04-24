/**
 * @file
 * @brief Player related debugging functions.
**/

#include "AppHdr.h"

#include "wiz-you.h"

#include <algorithm>
#include <functional>

#include "abyss.h"
#include "dbg-util.h"
#include "god-abil.h"
#include "god-wrath.h"
#include "item-use.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h" // frombool
#include "mutation.h"
#include "output.h"
#include "playable.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "skills.h"
#include "species.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "status.h"
#include "stringutil.h"
#include "tag-version.h"
#include "timed-effects.h" // zot clock
#include "transform.h"
#include "unicode.h"
#include "view.h"
#include "xom.h"

#ifdef WIZARD

job_type find_job_from_string(const string &str)
{
    const string spec = lowercase_string(str);

    job_type partial_match = JOB_UNKNOWN;

    for (const auto job : playable_jobs())
    {
        const auto name = lowercase_string(get_job_name(job));
        const auto pos = name.find(spec);

        if (pos == 0)
            return job;  // We prefer prefixes over partial matches.
        else if (pos != string::npos)
            partial_match = job;
    }

    return partial_match;
}

static xom_event_type _find_xom_event_from_string(const string &event_name)
{
    string spec = lowercase_string(event_name);

    xom_event_type x = XOM_DID_NOTHING;

    for (int i = 0; i <= XOM_LAST_REAL_ACT; ++i)
    {
        const xom_event_type xi = static_cast<xom_event_type>(i);
        const string x_name = lowercase_string(xom_effect_to_name(xi));

        string::size_type pos = x_name.find(spec);
        if (pos != string::npos)
        {
            if (pos == 0)
            {
                // We prefer prefixes over partial matches.
                x = xi;
                break;
            }
            else
                x = xi;
        }
    }

    return x;
}

void wizard_suppress()
{
    you.wizard = false;
    you.suppress_wizard = true;
    redraw_screen();
    update_screen();
}

void wizard_change_job_to(job_type job)
{
    you.char_class = job;
    you.chr_class_name = get_job_name(job);
}

void wizard_change_species()
{
    char specs[80];

    msgwin_get_line("What species would you like to be now? " ,
                    specs, sizeof(specs));

    if (specs[0] == '\0')
    {
        canned_msg(MSG_OK);
        return;
    }

    const species_type sp = species::from_str_loose(specs);

    // Means from_str_loose couldn't interpret `specs`.
    if (sp == SP_UNKNOWN)
    {
        mpr("That species isn't available.");
        return;
    }

    change_species_to(sp);
}

// Casts a specific spell by number or name.
void wizard_cast_spec_spell()
{
    char specs[80], *end;
    int spell;

    mprf(MSGCH_PROMPT, "Cast which spell? ");
    if (cancellable_get_line_autohist(specs, sizeof(specs))
        || specs[0] == '\0')
    {
        canned_msg(MSG_OK);
        crawl_state.cancel_cmd_repeat();
        return;
    }

    spell = strtol(specs, &end, 10);

    if (spell < 0 || end == specs)
    {
        if ((spell = spell_by_name(specs, true)) == SPELL_NO_SPELL)
        {
            mpr("Cannot find that spell.");
            crawl_state.cancel_cmd_repeat();
            return;
        }
    }

    if (your_spells(static_cast<spell_type>(spell), 0, false) == spret::abort)
        crawl_state.cancel_cmd_repeat();
}

void wizard_memorise_spec_spell()
{
    char specs[80], *end;
    int spell;

    mprf(MSGCH_PROMPT, "Memorise which spell? ");
    if (cancellable_get_line_autohist(specs, sizeof(specs))
        || specs[0] == '\0')
    {
        canned_msg(MSG_OK);
        crawl_state.cancel_cmd_repeat();
        return;
    }

    spell = strtol(specs, &end, 10);

    if (spell < 0 || end == specs)
    {
        if ((spell = spell_by_name(specs, true)) == SPELL_NO_SPELL)
        {
            mpr("Cannot find that spell.");
            crawl_state.cancel_cmd_repeat();
            return;
        }
    }

    if (get_spell_flags(static_cast<spell_type>(spell)) & spflag::monster)
        mpr("Spell is monster-only - unpredictable behaviour may result.");
    if (!learn_spell(static_cast<spell_type>(spell), true))
        crawl_state.cancel_cmd_repeat();
}

void wizard_heal(bool super_heal)
{
    if (super_heal)
    {
        mpr("Super healing.");
        // Clear more stuff.
        undrain_hp(9999);
        you.magic_contamination = 0;
        you.duration[DUR_LIQUID_FLAMES] = 0;
        you.clear_beholders();
        you.duration[DUR_PETRIFIED] = 0;
        you.duration[DUR_PETRIFYING] = 0;
        you.duration[DUR_CORROSION] = 0;
        you.duration[DUR_DOOM_HOWL] = 0;
        you.duration[DUR_WEAK] = 0;
        you.duration[DUR_NO_HOP] = 0;
        you.duration[DUR_LOCKED_DOWN] = 0;
        you.props["corrosion_amount"] = 0;
        you.duration[DUR_BREATH_WEAPON] = 0;
        delete_all_temp_mutations("Super heal");
        you.stat_loss.init(0);
        you.attribute[ATTR_STAT_LOSS_XP] = 0;
        decr_zot_clock();
        you.redraw_stats = true;
    }
    else
        mpr("Healing.");

    // Clear most status ailments.
    you.duration[DUR_SICKNESS]  = 0;
    you.duration[DUR_CONF]      = 0;
    you.duration[DUR_POISONING] = 0;
    you.duration[DUR_EXHAUSTED] = 0;
    set_hp(you.hp_max);
    set_mp(you.max_magic_points);
    you.redraw_hit_points = true;
    you.redraw_armour_class = true;
    you.redraw_evasion = true;

    for (int stat = 0; stat < NUM_STATS; stat++)
        you.duration[stat_zero_duration(static_cast<stat_type> (stat))] = 0;
}

void wizard_set_piety_to(int newpiety, bool force)
{
    if (newpiety < 0 || newpiety > MAX_PIETY)
    {
        mprf("Piety needs to be between 0 and %d.", MAX_PIETY);
        return;
    }
    if (newpiety > piety_breakpoint(5) && you_worship(GOD_RU))
    {
        mprf("Ru piety can't be greater than %d.", piety_breakpoint(5));
        return;
    }

    if (you_worship(GOD_XOM))
    {
        you.piety = newpiety;
        you.redraw_title = true; // redraw piety display

        int newinterest;
        if (!force)
        {
            char buf[30];

            // For Xom, also allow setting interest.
            mprf(MSGCH_PROMPT,
                 "Enter new interest (current = %d, Enter for 0): ",
                 you.gift_timeout);

            if (cancellable_get_line_autohist(buf, sizeof buf))
            {
                canned_msg(MSG_OK);
                return;
            }

            newinterest = atoi(buf);
        }
        else
            newinterest = newpiety;

        if (newinterest >= 0 && newinterest < 256)
            you.gift_timeout = newinterest;
        else
            mpr("Interest must be between 0 and 255.");

        mprf("Set piety to %d, interest to %d.", you.piety, newinterest);

        const string new_xom_favour = describe_xom_favour();
        const string msg = "You are now " + new_xom_favour;
        god_speaks(you.religion, msg.c_str());
        return;
    }

    if (newpiety < 1 && !force)
    {
        if (yesno("Are you sure you want to be excommunicated?", false, 'n'))
        {
            you.piety = 0;
            excommunication();
        }
        else
            canned_msg(MSG_OK);
        return;
    }
    mprf("Setting piety to %d.", newpiety);
    set_piety(newpiety);

    // Automatically reduce penance to 0.
    if (player_under_penance())
        dec_penance(you.penance[you.religion]);
}

void wizard_set_gold()
{
    const int default_gold = you.gold + 1000;
    mprf(MSGCH_PROMPT, "Enter new gold value (current = %d, Enter for %d): ",
         you.gold, default_gold);

    char buf[30];
    if (cancellable_get_line_autohist(buf, sizeof buf))
    {
        canned_msg(MSG_OK);
        return;
    }

    if (buf[0] == '\0')
        you.set_gold(default_gold);
    else
        you.set_gold(max(atoi(buf), 0));

    mprf("You now have %d gold piece%s.", you.gold, you.gold != 1 ? "s" : "");
}

void wizard_set_piety()
{
    if (you_worship(GOD_NO_GOD))
    {
        mpr("You are not religious!");
        return;
    }

    if (you_worship(GOD_RU))
    {
        mprf("Current progress to next sacrifice: %d  Progress needed: %d",
            you.props[RU_SACRIFICE_PROGRESS_KEY].get_int(),
            you.props[RU_SACRIFICE_DELAY_KEY].get_int());
    }

    mprf(MSGCH_PROMPT, "Enter new piety value (current = %d, Enter for 0): ",
         you.piety);
    char buf[30];
    if (cancellable_get_line_autohist(buf, sizeof buf))
    {
        canned_msg(MSG_OK);
        return;
    }

    wizard_set_piety_to(atoi(buf));
}

void wizard_exercise_skill()
{
    skill_type skill = debug_prompt_for_skill("Which skill (by name)? ");

    if (skill == SK_NONE)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr("Exercising...");
        exercise(skill, 10);
    }
}

void wizard_set_skill_level(skill_type skill)
{
    if (skill == SK_NONE)
        skill = debug_prompt_for_skill("Which skill (by name)? ");

    if (skill == SK_NONE)
    {
        mpr("That skill doesn't seem to exist.");
        return;
    }

    mpr(skill_name(skill));
    double amount = prompt_for_float("To what level? ");

    if (amount < 0 || amount > 27)
    {
        canned_msg(MSG_OK);
        return;
    }

    const int old_amount = you.skills[skill];

    set_skill_level(skill, amount);

    if (amount == 27)
    {
        you.train[skill] = TRAINING_DISABLED;
        you.train_alt[skill] = TRAINING_DISABLED;
        reset_training();
        check_selected_skills();
    }

    redraw_skill(skill);

    mprf("%s %s to skill level %.1f.", (old_amount < amount ? "Increased" :
                                      old_amount > amount ? "Lowered"
                                                          : "Reset"),
         skill_name(skill), amount);
}

void wizard_set_all_skills()
{
    double amount = prompt_for_float("Set all skills to what level? ");

    if (amount < 0)             // cancel returns -1 -- bwr
        canned_msg(MSG_OK);
    else
    {
        if (amount > 27)
            amount = 27;

        for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        {
            if (is_invalid_skill(sk) || is_useless_skill(sk))
                continue;

            set_skill_level(sk, amount);

            if (amount == 27)
            {
                you.train[sk] = TRAINING_DISABLED;
                you.training[sk] = 0;
            }
        }

        you.redraw_title = true;

        // We're not updating skill cost here since XP hasn't changed.

        calc_hp(true, false);
        calc_mp();

        you.redraw_armour_class = true;
        you.redraw_evasion = true;
    }
}

bool wizard_add_mutation()
{
    bool success = false;
    char specs[80];

    if (you.has_mutation(MUT_MUTATION_RESISTANCE))
        mpr("Ignoring mut resistance to apply mutation.");

    msgwin_get_line("Which mutation (name, 'good', 'bad', 'any', "
                    "'xom', 'slime', 'qazlal')? ",
                    specs, sizeof(specs));

    if (specs[0] == '\0')
    {
        canned_msg(MSG_OK);
        return true;
    }

    vector<mutation_type> partial_matches;
    mutation_type mutat = mutation_from_name(specs, true, &partial_matches);

    if (mutat >= CATEGORY_MUTATIONS)
         return mutate(mutat, "wizard power", true, true);

    if (mutat == NUM_MUTATIONS)
    {
        crawl_state.cancel_cmd_repeat();

        if (partial_matches.empty())
            mpr("No matching mutation names.");
        else
        {
            vector<string> matches;

            for (mutation_type mut : partial_matches)
            {
                const char *mutname = mutation_name(mut, true);
                ASSERT(mutname); // `mutation_name` returns nullptr if something went wrong getting the desc for `mut`.
                matches.emplace_back(mutname);
            }

            string prefix = make_stringf("No exact match for mutation '%s', possible matches are: ", specs);

            // Use mpr_comma_separated_list() because the list
            // might be *LONG*.
            mpr_comma_separated_list(prefix, matches, " and ", ", ",
                                     MSGCH_DIAGNOSTICS);
        }

        return false;
    }
    else
    {
        mprf("Found #%d: %s (\"%s\")", (int) mutat,
             mutation_name(mutat),
             mutation_desc(mutat, 1, false).c_str());

        const int levels =
            prompt_for_int("How many levels to increase or decrease? ",
                                  false);

        if (levels == 0)
        {
            canned_msg(MSG_OK);
            success = false;
        }
        else if (levels > 0)
        {
            for (int i = 0; i < levels; ++i)
                if (mutate(mutat, "wizard power", true, true))
                    success = true;
        }
        else
        {
            for (int i = 0; i < -levels; ++i)
                if (delete_mutation(mutat, "wizard power", true, true))
                    success = true;
        }
    }

    return success;
}

void wizard_set_abyss()
{
    char buf[80];
    mprf(MSGCH_PROMPT, "Enter values for X, Y, Z (space separated) or return: ");
    if (!cancellable_get_line_autohist(buf, sizeof buf))
        abyss_teleport();

    int x = 0, y = 0, z = 0;
    sscanf(buf, "%d %d %d", &x, &y, &z);
    set_abyss_state(coord_def(x,y), z);
}

void wizard_set_stats()
{
    char buf[80];
    mprf(MSGCH_PROMPT, "Enter values for Str, Int, Dex (space separated): ");
    if (cancellable_get_line_autohist(buf, sizeof buf) || buf[0] == '\0')
    {
        canned_msg(MSG_OK);
        return;
    }

    int sstr = you.strength(false),
        sdex = you.dex(false),
        sint = you.intel(false);

    sscanf(buf, "%d %d %d", &sstr, &sint, &sdex);

    mprf("Setting attributes (Str, Int, Dex) to: %i, %i, %i", sstr, sint, sdex);
    you.base_stats[STAT_STR] = debug_cap_stat(sstr);
    you.base_stats[STAT_INT] = debug_cap_stat(sint);
    you.base_stats[STAT_DEX] = debug_cap_stat(sdex);
    you.stat_loss.init(0);
    you.attribute[ATTR_STAT_LOSS_XP] = 0;
    you.redraw_stats.init(true);
    you.redraw_evasion = true;
}

void wizard_edit_durations()
{
    vector<duration_type> durs;
    size_t max_len = 0;

    for (int i = 0; i < NUM_DURATIONS; ++i)
    {
        const duration_type dur = static_cast<duration_type>(i);

        if (!you.duration[i])
            continue;

        max_len = max(strlen(duration_name(dur)), max_len);
        durs.push_back(dur);
    }

    if (!durs.empty())
    {
        for (size_t i = 0; i < durs.size(); ++i)
        {
            const duration_type dur = durs[i];
            const char ch = i >= 26 ? ' ' : 'a' + i;
            mprf_nocap(MSGCH_PROMPT, "%c%c %-*s : %d",
                 ch, ch == ' ' ? ' ' : ')',
                 (int)max_len, duration_name(dur), you.duration[dur]);
        }
        mprf(MSGCH_PROMPT, "\nEdit which duration (letter or name)? ");
    }
    else
        mprf(MSGCH_PROMPT, "Edit which duration (name)? ");

    char buf[80];

    if (cancellable_get_line_autohist(buf, sizeof buf) || !*buf)
    {
        canned_msg(MSG_OK);
        return;
    }

    if (!strlcpy(buf, lowercase_string(trimmed_string(buf)).c_str(), sizeof(buf)))
    {
        canned_msg(MSG_OK);
        return;
    }

    duration_type choice = NUM_DURATIONS;

    if (strlen(buf) == 1 && isalower(buf[0]))
    {
        if (durs.empty())
        {
            mprf(MSGCH_PROMPT, "No existing durations to choose from.");
            return;
        }
        const int dchoice = buf[0] - 'a';

        if (dchoice < 0 || dchoice >= (int) durs.size())
        {
            mprf(MSGCH_PROMPT, "Invalid choice.");
            return;
        }
        choice = durs[dchoice];
    }
    else
    {
        vector<duration_type> matches;
        vector<string> match_names;

        for (int i = 0; i < NUM_DURATIONS; ++i)
        {
            const duration_type dur = static_cast<duration_type>(i);
            if (strcmp(duration_name(dur), buf) == 0)
            {
                choice = dur;
                break;
            }
            if (strstr(duration_name(dur), buf) != nullptr)
            {
                matches.push_back(dur);
                match_names.emplace_back(duration_name(dur));
            }
        }
        if (choice != NUM_DURATIONS)
            ;
        else if (matches.size() == 1)
            choice = matches[0];
        else if (matches.empty())
        {
            mprf(MSGCH_PROMPT, "No durations matching '%s'.", buf);
            return;
        }
        else
        {
            string prefix = "No exact match for duration '";
            prefix += buf;
            prefix += "', possible matches are: ";

            mpr_comma_separated_list(prefix, match_names, " and ", ", ",
                                     MSGCH_DIAGNOSTICS);
            return;
        }
    }

    snprintf(buf, sizeof(buf), "Set '%s' to: ", duration_name(choice));
    int num = prompt_for_int(buf, false);

    if (num == 0)
    {
        mprf(MSGCH_PROMPT, "Can't set duration directly to 0, setting it to 1 instead.");
        num = 1;
    }
    you.duration[choice] = num;
}

void wizard_list_props()
{
    mprf(MSGCH_DIAGNOSTICS, "props: %s",
         you.describe_props().c_str());
}

/*
 * Hard reset Ds mutations for `xl` based on the trait schedule.
 */
static void reset_ds_muts_from_schedule(int xl)
{
    if (you.species != SP_DEMONSPAWN)
        return;

    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        int innate_levels = 0;
        bool is_trait = false;
        for (player::demon_trait trait : you.demonic_traits)
        {
            if (trait.mutation == mut)
            {
                is_trait = true;
                if (xl >= trait.level_gained)
                    innate_levels += 1;
            }
        }
        if (is_trait)
        {
            while (you.innate_mutation[mut] > innate_levels)
            {
                // First set it as non-innate, then delete the mutation from
                // there. delete_mutation won't delete mutations otherwise.
                // This step doesn't affect temporary mutations.
                you.innate_mutation[mut]--;
                delete_mutation(mut, "level change", false, true, false, false);
            }
            if (you.innate_mutation[mut] < innate_levels)
                perma_mutate(mut, innate_levels - you.innate_mutation[mut], "level change");
        }
    }
}


static void debug_uptick_xl(int newxl, bool train)
{
    if (train)
    {
        you.exp_available += 10 * (exp_needed(newxl) - you.experience);
        train_skills();
    }
    you.experience = exp_needed(newxl);
    if (you.experience_level < you.max_level)
    {
        // Fixing up Ds muts needs to happen before the level change, so that the mutations validate correctly
        if (newxl < you.max_level)
            reset_ds_muts_from_schedule(newxl);
        else
            reset_ds_muts_from_schedule(you.max_level); // let level change handle the rest
    }
    level_change(true);
#ifdef DEBUG
    validate_mutations();
#endif
}

/**
 * Set the player's XL to a new value.
 * @param newxl  The new experience level.
 */
static void debug_downtick_xl(int newxl)
{
    set_hp(you.hp_max);
    // boost maxhp so we don't die if heavily drained
    you.hp_max_adj_perm += 1000;
    you.experience = exp_needed(newxl);
    level_change();
    reset_ds_muts_from_schedule(newxl); // needs to happen after the level change
    you.skill_cost_level = 0;
    check_skill_cost_change();
    // restore maxhp loss
    you.hp_max_adj_perm -= 1000;
    calc_hp();
    if (you.hp_max <= 0)
    {
        // ... but remove it completely if unviable
        you.hp_max_adj_temp = max(you.hp_max_adj_temp, 0);
        you.hp_max_adj_perm = max(you.hp_max_adj_perm, 0);
        calc_hp();
    }

    set_hp(max(1, you.hp));
#ifdef DEBUG
    validate_mutations();
#endif
}

void wizard_set_xl(bool change_skills)
{
    mprf(MSGCH_PROMPT, "Enter new experience level: ");
    char buf[30];
    if (cancellable_get_line_autohist(buf, sizeof buf))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int newxl = atoi(buf);
    if (newxl < 1 || newxl > you.get_max_xl() || newxl == you.experience_level)
    {
        canned_msg(MSG_OK);
        return;
    }

    set_xl(newxl, change_skills);
    mprf("Experience level set to %d.", newxl);
}

void set_xl(const int newxl, const bool train, const bool silent)
{
    msg::suppress mx(silent);

    if (newxl < you.experience_level)
        debug_downtick_xl(newxl);
    else
        debug_uptick_xl(newxl, train);
}

void wizard_get_god_gift()
{
    if (you_worship(GOD_NO_GOD))
    {
        mpr("You are not religious!");
        return;
    }

    if (you_worship(GOD_RU))
    {
        ru_offer_new_sacrifices();
        return;
    }

    if (you_worship(GOD_ASHENZARI))
    {
        ashenzari_offer_new_curse();
        return;
    }

    if (!do_god_gift(true))
        mpr("Nothing happens.");
}

void wizard_toggle_xray_vision()
{
    you.wizard_vision = !you.wizard_vision;
    mprf("X-ray vision %s.", you.wizard_vision ? "enabled" : "disabled");
    viewwindow(true);
    update_screen();
}

void wizard_freeze_time()
{
    auto& props = you.props;
    // this property is never false: either true or unset
    if (props.exists(FREEZE_TIME_KEY))
    {
        props.erase(FREEZE_TIME_KEY);
        mpr("You allow the flow of time to resume.");
    }
    else
    {
        props[FREEZE_TIME_KEY] = true;
        mpr("You bring the flow of time to a stop.");
    }
}

void wizard_god_wrath()
{
    const god_type god = choose_god(you.religion);
    if (god == NUM_GODS)
        mpr("That god doesn't seem to exist!");
    else if (god == GOD_NO_GOD)
        mpr("You suffer the terrible wrath of No God.");
    else if (!divine_retribution(god, true, true))
    {
        // Dead Jiyva, or Ru/Gozag/Ashenzari.
        mprf("%s is not feeling wrathful today.", god_name(god).c_str());
    }
}

void wizard_god_mollify()
{
    bool mollified = false;
    for (god_iterator it; it; ++it)
    {
        if (player_under_penance(*it))
        {
            dec_penance(*it, you.penance[*it]);
            mollified = true;
        }
    }
    if (!mollified)
        mpr("You are not under penance.");
}

void wizard_transform()
{
    transformation form;

    while (true)
    {
        string line;
        for (int i = 0; i < NUM_TRANSFORMS; i++)
        {
            const auto tr = static_cast<transformation>(i);
#if TAG_MAJOR_VERSION == 34
            if (tr == transformation::jelly || tr == transformation::porcupine)
                continue;
#endif
            line += make_stringf("[%c] %-10s ", i + 'a', transform_name(tr));
            if (i % 5 == 4 || i == NUM_TRANSFORMS - 1)
            {
                mprf(MSGCH_PROMPT, "%s", line.c_str());
                line.clear();
            }
        }
        mprf(MSGCH_PROMPT, "Which form (ESC to exit)? ");

        int keyin = toalower(get_ch());

        if (key_is_escape(keyin) || keyin == ' '
            || keyin == '\r' || keyin == '\n')
        {
            canned_msg(MSG_OK);
            return;
        }

        if (keyin < 'a' || keyin > 'a' + NUM_TRANSFORMS - 1)
            continue;

        const auto k_tr = static_cast<transformation>(keyin - 'a');
#if TAG_MAJOR_VERSION == 34
        if (k_tr == transformation::jelly || k_tr == transformation::porcupine)
            continue;
#endif
        form = k_tr;
        break;
    }

    you.transform_uncancellable = false;
    if (!transform(200, form) && you.form != form)
        mpr("Transformation failed.");
}

void wizard_join_religion()
{
    if (you.has_mutation(MUT_FORLORN))
    {
        mpr("Not even in wizmode may divine creatures worship a god!");
        return;
    }
    god_type god = choose_god();
    if (god == NUM_GODS)
        mpr("That god doesn't seem to exist!");
    else if (god == GOD_NO_GOD)
    {
        if (you_worship(GOD_NO_GOD))
            mpr("You already have no god!");
        else
            excommunication();
    }
    else if (you_worship(god))
        mpr("You already worship that god!");
    else
    {
        if (god == GOD_GOZAG)
            you.gold = max(you.gold, gozag_service_fee());
        join_religion(god);
    }
}

void wizard_xom_acts()
{
    char specs[80];

    msgwin_get_line("What action should Xom take? (Blank = any) " ,
                    specs, sizeof(specs));

    const int severity = you_worship(GOD_XOM) ? abs(you.piety - HALF_MAX_PIETY)
                                              : random_range(0, HALF_MAX_PIETY);

    if (specs[0] == '\0')
    {
        const maybe_bool nice = you_worship(GOD_XOM) ? MB_MAYBE :
                                frombool(coinflip());
        const xom_event_type result = xom_acts(severity, nice);
        dprf("Xom did '%s'.", xom_effect_to_name(result).c_str());
#ifndef DEBUG_DIAGNOSTICS
        UNUSED(result);
#endif
        return;
    }

    xom_event_type event = _find_xom_event_from_string(specs);
    if (event == XOM_DID_NOTHING)
    {
        dprf("That action doesn't seem to exist!");
        return;
    }

    dprf("Okay, Xom is doing '%s'.", xom_effect_to_name(event).c_str());
    xom_take_action(event, severity);
}

void wizard_set_zot_clock()
{
    const int max_zot_clock = MAX_ZOT_CLOCK / BASELINE_DELAY;

    string prompt = make_stringf("Enter new Zot clock value "
                                 "(current = %d, from 0 to %d): ",
                                 turns_until_zot(), max_zot_clock);
    int turns_left = prompt_for_int(prompt.c_str(), true);

    if (turns_left == -1)
        canned_msg(MSG_OK);
    else if (turns_left > max_zot_clock)
        mprf("Zot clock should be between 0 and %d", max_zot_clock);
    else
        set_turns_until_zot(turns_left);
}
#endif

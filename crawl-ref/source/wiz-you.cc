/**
 * @file
 * @brief Player related debugging functions.
**/

#include "AppHdr.h"

#include "wiz-you.h"

#include "abyss.h"

#include "cio.h"
#include "dbg-util.h"
#include "food.h"
#include "godprayer.h"
#include "godwrath.h"
#include "libutil.h"
#include "message.h"
#include "mutation.h"
#include "newgame.h"
#include "ng-setup.h"
#include "player.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"
#include "unicode.h"
#include "xom.h"

#ifdef WIZARD
void wizard_change_species(void)
{
    char specs[80];
    int i;

    msgwin_get_line("What species would you like to be now? " ,
                    specs, sizeof(specs));

    if (specs[0] == '\0')
        return;
    string spec = lowercase_string(specs);

    species_type sp = SP_UNKNOWN;

    for (i = 0; i < NUM_SPECIES; ++i)
    {
        const species_type si = static_cast<species_type>(i);
        const string sp_name = lowercase_string(species_name(si));

        string::size_type pos = sp_name.find(spec);
        if (pos != string::npos)
        {
            if (pos == 0)
            {
                // We prefer prefixes over partial matches.
                sp = si;
                break;
            }
            else
                sp = si;
        }
    }

    // Can't use magic cookies or placeholder species.
    if (!is_valid_species(sp))
    {
        mpr("That species isn't available.");
        return;
    }

    // Re-scale skill-points.
    for (i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        you.skill_points[i] *= species_apt_factor(sk, sp)
                               / species_apt_factor(sk);
    }

    you.species = sp;
    you.is_undead = get_undead_state(sp);

    // Change permanent mutations, but preserve non-permanent ones.
    uint8_t prev_muts[NUM_MUTATIONS];
    for (i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (you.innate_mutations[i] > 0)
        {
            if (you.innate_mutations[i] > you.mutation[i])
                you.mutation[i] = 0;
            else
                you.mutation[i] -= you.innate_mutations[i];

            you.innate_mutations[i] = 0;
        }
        prev_muts[i] = you.mutation[i];
    }
    give_basic_mutations(sp);
    for (i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (prev_muts[i] > you.innate_mutations[i])
            you.innate_mutations[i] = 0;
        else
            you.innate_mutations[i] -= prev_muts[i];
    }

    switch (sp)
    {
    case SP_RED_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_HEAT_RESISTANCE, 1, "wizard race change");
        break;

    case SP_WHITE_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_COLD_RESISTANCE, 1, "wizard race change");
        break;

    case SP_GREEN_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_POISON_RESISTANCE, 1, "wizard race change");
        if (you.experience_level >= 14)
            perma_mutate(MUT_STINGER, 1, "wizard race change");
        break;

    case SP_YELLOW_DRACONIAN:
        if (you.experience_level >= 14)
            perma_mutate(MUT_ACIDIC_BITE, 1, "wizard race change");
        break;

    case SP_GREY_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_UNBREATHING, 1, "wizard race change");
        break;

    case SP_BLACK_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_SHOCK_RESISTANCE, 1, "wizard race change");
        if (you.experience_level >= 14)
            perma_mutate(MUT_BIG_WINGS, 1, "wizard race change");
        break;

    case SP_DEMONSPAWN:
    {
        roll_demonspawn_mutations();
        for (i = 0; i < int(you.demonic_traits.size()); ++i)
        {
            mutation_type m = you.demonic_traits[i].mutation;

            if (you.demonic_traits[i].level_gained > you.experience_level)
                continue;

            ++you.mutation[m];
            ++you.innate_mutations[m];
        }
        break;
    }

    case SP_DEEP_DWARF:
        if (you.experience_level >= 9)
            perma_mutate(MUT_PASSIVE_MAPPING, 1, "wizard race change");
        if (you.experience_level >= 14)
            perma_mutate(MUT_NEGATIVE_ENERGY_RESISTANCE, 1, "wizard race change");
        if (you.experience_level >= 18)
            perma_mutate(MUT_PASSIVE_MAPPING, 1, "wizard race change");
        break;

    case SP_FELID:
        if (you.experience_level >= 6)
            perma_mutate(MUT_SHAGGY_FUR, 1, "wizard race change");
        if (you.experience_level >= 12)
            perma_mutate(MUT_SHAGGY_FUR, 1, "wizard race change");
        break;

    default:
        break;
    }

    // Sanitize skills.
    fixup_skills();

    // Could delete only inappropriate ones, but meh.
    you.sage_skills.clear();
    you.sage_xp.clear();
    you.sage_bonus.clear();

    calc_hp();
    calc_mp();

    burden_change();
    // The player symbol depends on species.
    update_player_symbol();
#ifdef USE_TILE
    init_player_doll();
#endif
    redraw_screen();
}
#endif

#ifdef WIZARD
// Casts a specific spell by number or name.
void wizard_cast_spec_spell(void)
{
    char specs[80], *end;
    int spell;

    mpr("Cast which spell? ", MSGCH_PROMPT);
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

    if (your_spells(static_cast<spell_type>(spell), 0, false) == SPRET_ABORT)
        crawl_state.cancel_cmd_repeat();
}

void wizard_memorise_spec_spell(void)
{
    char specs[80], *end;
    int spell;

    mpr("Memorise which spell? ", MSGCH_PROMPT);
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

    if (!learn_spell(static_cast<spell_type>(spell)))
        crawl_state.cancel_cmd_repeat();
}
#endif

void wizard_heal(bool super_heal)
{
    if (super_heal)
    {
        // Clear more stuff and give a HP boost.
        unrot_hp(9999);
        you.magic_contamination = 0;
        you.duration[DUR_LIQUID_FLAMES] = 0;
        you.clear_beholders();
        inc_max_hp(10);
        you.attribute[ATTR_XP_DRAIN] = 0;
    }

    // Clear most status ailments.
    you.rotting = 0;
    you.disease = 0;
    you.duration[DUR_CONF]      = 0;
    you.duration[DUR_MISLED]    = 0;
    you.duration[DUR_POISONING] = 0;
    you.duration[DUR_EXHAUSTED] = 0;
    set_hp(you.hp_max);
    set_mp(you.max_magic_points);
    set_hunger(10999, true);
    you.redraw_hit_points = true;
}

void wizard_set_hunger_state()
{
    string hunger_prompt =
        "Set hunger state to s(T)arving, (N)ear starving, (H)ungry";
    if (you.species == SP_GHOUL)
        hunger_prompt += " or (S)atiated";
    else
        hunger_prompt += ", (S)atiated, (F)ull or (E)ngorged";
    hunger_prompt += "? ";

    mprf(MSGCH_PROMPT, "%s", hunger_prompt.c_str());

    const int c = toalower(getchk());

    // Values taken from food.cc.
    switch (c)
    {
    case 't': you.hunger = HUNGER_STARVING / 2;   break;
    case 'n': you.hunger = 1200;  break;
    case 'h': you.hunger = 2400;  break;
    case 's': you.hunger = 5000;  break;
    case 'f': you.hunger = 8000;  break;
    case 'e': you.hunger = HUNGER_MAXIMUM; break;
    default:  canned_msg(MSG_OK); break;
    }

    food_change();

    if (you.species == SP_GHOUL && you.hunger_state >= HS_SATIATED)
        mpr("Ghouls can never be full or above!");
}

void wizard_set_piety()
{
    if (you_worship(GOD_NO_GOD))
    {
        mpr("You are not religious!");
        return;
    }

    mprf(MSGCH_PROMPT, "Enter new piety value (current = %d, Enter for 0): ",
         you.piety);
    char buf[30];
    if (cancellable_get_line_autohist(buf, sizeof buf))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int newpiety = atoi(buf);
    if (newpiety < 0 || newpiety > 200)
    {
        mpr("Piety needs to be between 0 and 200.");
        return;
    }

    if (you_worship(GOD_XOM))
    {
        you.piety = newpiety;

        // For Xom, also allow setting interest.
        mprf(MSGCH_PROMPT, "Enter new interest (current = %d, Enter for 0): ",
             you.gift_timeout);

        if (cancellable_get_line_autohist(buf, sizeof buf))
        {
            canned_msg(MSG_OK);
            return;
        }
        const int newinterest = atoi(buf);
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

    if (newpiety < 1)
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

    // We have to set the exact piety value this way, because diff may
    // be decreased to account for things like penance and gift timeout.
    int diff;
    do
    {
        diff = newpiety - you.piety;
        if (diff > 0)
            gain_piety(diff, 1, true, false);
        else if (diff < 0)
            lose_piety(-diff);
    }
    while (diff != 0);

    // Automatically reduce penance to 0.
    if (player_under_penance())
        dec_penance(you.penance[you.religion]);
}

//---------------------------------------------------------------
//
// debug_add_skills
//
//---------------------------------------------------------------
#ifdef WIZARD
void wizard_exercise_skill(void)
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
#endif

#ifdef WIZARD
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
        you.train[skill] = 0;
        you.train_alt[skill] = 0;
        reset_training();
        check_selected_skills();
    }

    redraw_skill(skill);

    mprf("%s %s to skill level %.1f.", (old_amount < amount ? "Increased" :
                                      old_amount > amount ? "Lowered"
                                                          : "Reset"),
         skill_name(skill), amount);
}
#endif


#ifdef WIZARD
void wizard_set_all_skills(void)
{
    double amount = prompt_for_float("Set all skills to what level? ");

    if (amount < 0)             // cancel returns -1 -- bwr
        canned_msg(MSG_OK);
    else
    {
        if (amount > 27)
            amount = 27;

        for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
        {
            skill_type sk = static_cast<skill_type>(i);
            if (is_invalid_skill(sk) || is_useless_skill(sk))
                continue;

            set_skill_level(sk, amount);

            if (amount == 27)
            {
                you.train[sk] = 0;
                you.training[sk] = 0;
            }
        }

        you.redraw_title = true;

        // We're not updating skill cost here since XP hasn't changed.

        calc_hp();
        calc_mp();

        you.redraw_armour_class = true;
        you.redraw_evasion = true;
    }
}
#endif

#ifdef WIZARD
bool wizard_add_mutation()
{
    bool success = false;
    char specs[80];

    if (player_mutation_level(MUT_MUTATION_RESISTANCE) > 0
        && !crawl_state.is_replaying_keys())
    {
        const char* msg;

        if (you.mutation[MUT_MUTATION_RESISTANCE] == 3)
            msg = "You are immune to mutations; remove immunity?";
        else
            msg = "You are resistant to mutations; remove resistance?";

        if (yesno(msg, true, 'n'))
        {
            you.mutation[MUT_MUTATION_RESISTANCE] = 0;
            crawl_state.cancel_cmd_repeat();
        }
    }

    int answer = yesnoquit("Force mutation to happen?", true, 'n');
    if (answer == -1)
    {
        canned_msg(MSG_OK);
        return false;
    }
    const bool force = (answer == 1);

    if (player_mutation_level(MUT_MUTATION_RESISTANCE) == 3 && !force)
    {
        mpr("Can't mutate when immune to mutations without forcing it.");
        crawl_state.cancel_cmd_repeat();
        return false;
    }

    answer = yesnoquit("Treat mutation as god gift?", true, 'n');
    if (answer == -1)
    {
        canned_msg(MSG_OK);
        return false;
    }
    const bool god_gift = (answer == 1);

    msgwin_get_line("Which mutation (name, 'good', 'bad', 'any', "
                    "'xom', 'slime')? ",
                    specs, sizeof(specs));

    if (specs[0] == '\0')
        return false;

    string spec = lowercase_string(specs);

    mutation_type mutat = NUM_MUTATIONS;

    if (spec == "good")
        mutat = RANDOM_GOOD_MUTATION;
    else if (spec == "bad")
        mutat = RANDOM_BAD_MUTATION;
    else if (spec == "any")
        mutat = RANDOM_MUTATION;
    else if (spec == "xom")
        mutat = RANDOM_XOM_MUTATION;
    else if (spec == "slime")
        mutat = RANDOM_SLIME_MUTATION;

    if (mutat != NUM_MUTATIONS)
    {
        int old_resist = player_mutation_level(MUT_MUTATION_RESISTANCE);

        success = mutate(mutat, "wizard power", true, force, god_gift);

        if (old_resist < player_mutation_level(MUT_MUTATION_RESISTANCE)
            && !force)
        {
            crawl_state.cancel_cmd_repeat("Your mutation resistance has "
                                          "increased.");
        }
        return success;
    }

    vector<mutation_type> partial_matches;

    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        if (!is_valid_mutation(mut))
            continue;

        const mutation_def& mdef = get_mutation_def(mut);
        if (spec == mdef.wizname)
        {
            mutat = mut;
            break;
        }

        if (strstr(mdef.wizname, spec.c_str()))
            partial_matches.push_back(mut);
    }

    // If only one matching mutation, use that.
    if (mutat == NUM_MUTATIONS && partial_matches.size() == 1)
        mutat = partial_matches[0];

    if (mutat == NUM_MUTATIONS)
    {
        crawl_state.cancel_cmd_repeat();

        if (partial_matches.empty())
            mpr("No matching mutation names.");
        else
        {
            vector<string> matches;

            for (unsigned int i = 0; i < partial_matches.size(); ++i)
                matches.push_back(get_mutation_def(partial_matches[i]).wizname);

            string prefix = "No exact match for mutation '" +
                            spec +  "', possible matches are: ";

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
             get_mutation_def(mutat).wizname,
             mutation_name(mutat, 1, false).c_str());

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
                if (mutate(mutat, "wizard power", true, force, god_gift))
                    success = true;
        }
        else
        {
            for (int i = 0; i < -levels; ++i)
                if (delete_mutation(mutat, "wizard power", true, force, god_gift))
                    success = true;
        }
    }

    return success;
}
#endif

void wizard_set_abyss()
{
    char buf[80];
    mprf(MSGCH_PROMPT, "Enter values for X, Y, Z (space separated) or return: ");
    if (!cancellable_get_line_autohist(buf, sizeof buf))
        abyss_teleport(true);

    uint32_t x = 0, y = 0, z = 0;
    sscanf(buf, "%d %d %d", &x, &y, &z);
    set_abyss_state(coord_def(x,y), z);
}

void wizard_set_stats()
{
    char buf[80];
    mprf(MSGCH_PROMPT, "Enter values for Str, Int, Dex (space separated): ");
    if (cancellable_get_line_autohist(buf, sizeof buf))
        return;

    int sstr = you.strength(false),
        sdex = you.dex(false),
        sint = you.intel(false);

    sscanf(buf, "%d %d %d", &sstr, &sint, &sdex);

    you.base_stats[STAT_STR] = debug_cap_stat(sstr);
    you.base_stats[STAT_INT] = debug_cap_stat(sint);
    you.base_stats[STAT_DEX] = debug_cap_stat(sdex);
    you.stat_loss.init(0);
    you.redraw_stats.init(true);
    you.redraw_evasion = true;
}

static const char* dur_names[] =
{
    "invis",
    "conf",
    "paralysis",
    "slow",
    "mesmerised",
    "haste",
    "might",
    "brilliance",
    "agility",
    "flight",
    "berserker",
    "poisoning",
    "confusing touch",
    "sure blade",
    "corona",
    "deaths door",
    "fire shield",
    "building rage",
    "exhausted",
    "liquid flames",
    "icy armour",
    "repel missiles",
    "prayer",
    "piety pool",
    "divine vigour",
    "divine stamina",
    "divine shield",
    "regeneration",
    "swiftness",
#if TAG_MAJOR_VERSION == 34
    "controlled flight",
#endif
    "teleport",
    "control teleport",
    "breath weapon",
    "transformation",
    "death channel",
    "deflect missiles",
    "phase shift",
#if TAG_MAJOR_VERSION == 34
    "see invisible",
#endif
    "weapon brand",
    "demonic guardian",
    "pbd",
    "silence",
    "condensation shield",
    "stoneskin",
    "gourmand",
    "bargain",
#if TAG_MAJOR_VERSION == 34
    "insulation",
#endif
    "resistance",
    "slaying",
    "stealth",
    "magic shield",
    "sleep",
    "telepathy",
    "petrified",
    "lowered mr",
    "repel stairs move",
    "repel stairs climb",
    "coloured smoke trail",
    "slimify",
    "time step",
    "icemail depleted",
    "misled",
    "quad damage",
    "afraid",
    "mirror damage",
    "scrying",
    "tornado",
    "liquefying",
    "heroism",
    "finesse",
    "lifesaving",
    "paralysis immunity",
    "darkness",
    "petrifying",
    "shrouded",
    "tornado cooldown",
#if TAG_MAJOR_VERSION == 34
    "nausea",
#endif
    "ambrosia",
#if TAG_MAJOR_VERSION == 34
    "temporary mutations",
#endif
    "disjunction",
    "vehumet gift",
#if TAG_MAJOR_VERSION == 34
    "battlesphere",
#endif
    "sentinel's mark",
    "sickening",
    "drowning",
    "drowning immunity",
    "flayed",
    "retching",
    "weak",
    "dimension anchor",
    "antimagic",
    "spirit howl",
    "infused",
    "song of slaying",
    "song of shielding",
    "toxic radiance",
    "reciting",
    "grasping roots",
    "sleep immunity",
};

void wizard_edit_durations(void)
{
    COMPILE_CHECK(ARRAYSZ(dur_names) == NUM_DURATIONS);
    vector<int> durs;
    size_t max_len = 0;

    for (int i = 0; i < NUM_DURATIONS; ++i)
    {
        if (!you.duration[i])
            continue;

        max_len = max(strlen(dur_names[i]), max_len);
        durs.push_back(i);
    }

    if (!durs.empty())
    {
        for (unsigned int i = 0; i < durs.size(); ++i)
        {
            int dur = durs[i];
            mprf_nocap(MSGCH_PROMPT, "%c) %-*s : %d", 'a' + i, (int)max_len,
                 dur_names[dur], you.duration[dur]);
        }
        mpr("", MSGCH_PROMPT);
        mpr("Edit which duration (letter or name)? ", MSGCH_PROMPT);
    }
    else
        mpr("Edit which duration (name)? ", MSGCH_PROMPT);

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

    int choice = -1;

    if (strlen(buf) == 1)
    {
        if (durs.empty())
        {
            mpr("No existing durations to choose from.", MSGCH_PROMPT);
            return;
        }
        choice = buf[0] - 'a';

        if (choice < 0 || choice >= (int) durs.size())
        {
            mpr("Invalid choice.", MSGCH_PROMPT);
            return;
        }
        choice = durs[choice];
    }
    else
    {
        vector<int>    matches;
        vector<string> match_names;

        for (int i = 0; i < NUM_DURATIONS; ++i)
        {
            if (strcmp(dur_names[i], buf) == 0)
            {
                choice = i;
                break;
            }
            if (strstr(dur_names[i], buf) != NULL)
            {
                matches.push_back(i);
                match_names.push_back(dur_names[i]);
            }
        }
        if (choice != -1)
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

    snprintf(buf, sizeof(buf), "Set '%s' to: ", dur_names[choice]);
    int num = prompt_for_int(buf, false);

    if (num == 0)
    {
        mpr("Can't set duration directly to 0, setting it to 1 instead.",
            MSGCH_PROMPT);
        num = 1;
    }
    you.duration[choice] = num;
}

static void debug_uptick_xl(int newxl, bool train)
{
    if (train)
    {
        you.exp_available += exp_needed(newxl) - you.experience;
        train_skills();
    }
    you.experience = exp_needed(newxl);
    level_change(NON_MONSTER, NULL, true);
}

static void debug_downtick_xl(int newxl)
{
    you.hp = you.hp_max;
    you.hp_max_perm += 1000; // boost maxhp so we don't die if heavily rotted
    you.experience = exp_needed(newxl);
    level_change();
    you.skill_cost_level = 0;
    check_skill_cost_change();
    // restore maxhp loss
    you.hp_max_perm -= 1000;
    calc_hp();
    if (you.hp_max <= 0)
    {
        // ... but remove it completely if unviable
        you.hp_max_temp = max(you.hp_max_temp, 0);
        you.hp_max_perm = max(you.hp_max_perm, 0);
        calc_hp();
    }

    you.hp       = max(1, you.hp);
}

void wizard_set_xl()
{
    mprf(MSGCH_PROMPT, "Enter new experience level: ");
    char buf[30];
    if (cancellable_get_line_autohist(buf, sizeof buf))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int newxl = atoi(buf);
    if (newxl < 1 || newxl > 27 || newxl == you.experience_level)
    {
        canned_msg(MSG_OK);
        return;
    }

    set_xl(newxl, yesno("Train skills?", true, 'n'));
    mprf("Experience level set to %d.", newxl);
}

void set_xl(const int newxl, const bool train)
{
    no_messages mx;
    if (newxl < you.experience_level)
        debug_downtick_xl(newxl);
    else
        debug_uptick_xl(newxl, train);
}

void wizard_get_god_gift(void)
{
    if (you_worship(GOD_NO_GOD))
    {
        mpr("You are not religious!");
        return;
    }

    if (!do_god_gift(true))
        mpr("Nothing happens.");
}

void wizard_toggle_xray_vision()
{
    you.xray_vision = !you.xray_vision;
    viewwindow(true);
}

void wizard_god_wrath()
{
    if (you_worship(GOD_NO_GOD))
    {
        mpr("You suffer the terrible wrath of No God.");
        return;
    }

    if (!divine_retribution(you.religion, true, true))
        // Currently only dead Jiyva.
        mpr("You're not eligible for wrath.");
}

void wizard_god_mollify()
{
    for (int i = GOD_NO_GOD; i < NUM_GODS; ++i)
    {
        if (player_under_penance((god_type) i))
            dec_penance((god_type) i, you.penance[i]);
    }
}

void wizard_transform()
{
    transformation_type form;

    while (true)
    {
        string line;
        for (int i = 0; i <= LAST_FORM; i++)
        {
            line += make_stringf("[%c] %-10s ", i + 'a',
                                 transform_name((transformation_type)i));
            if (i % 5 == 4 || i == LAST_FORM)
            {
                mpr(line, MSGCH_PROMPT);
                line.clear();
            }
        }
        mpr("Which form (ESC to exit)? ", MSGCH_PROMPT);

        int keyin = toalower(get_ch());

        if (key_is_escape(keyin) || keyin == ' '
            || keyin == '\r' || keyin == '\n')
        {
            canned_msg(MSG_OK);
            return;
        }

        if (keyin < 'a' || keyin > 'a' + LAST_FORM)
            continue;

        form = (transformation_type)(keyin - 'a');

        break;
    }

    you.transform_uncancellable = false;
    if (!transform(200, form) && you.form != form)
        mpr("Transformation failed.");
}

static void _wizard_modify_character(string inputdata)
// for now this just sets skill levels and str dex int
// (this should be enough to debug with)
{
    vector<string>  tokens = split_string(" ", inputdata);
    int size = tokens.size();
    if (size > 3 && tokens[1] == "Level") // + Level 4.0 Fighting
    {
        skill_type skill = skill_from_name(lowercase_string(tokens[3]).c_str());
        double amount = atof(tokens[2].c_str());
        set_skill_level(skill, amount);
        if (tokens[0] == "+")
            you.train[skill] = 1;
        else if (tokens[0] == "*")
            you.train[skill] = 2;
        else
            you.train[skill] = 0;

        redraw_skill(skill);

        return;
    }

    if (size > 5 && tokens[0] == "HP") // HP 23/23 AC 3 Str 21 XL: 1 Next: 0%
    {
        for (int k = 1; k < size; k++)
        {
            if (tokens[k] == "Str")
            {
                you.base_stats[STAT_STR] = debug_cap_stat(atoi(tokens[k+1].c_str()));
                you.redraw_stats.init(true);
                you.redraw_evasion = true;
                return;
            }
        }
    }

    if (size > 5 && tokens[0] == "MP")
    {
        for (int k = 1; k < size; k++)
        {
            if (tokens[k] == "Int")
            {
                you.base_stats[STAT_INT] = debug_cap_stat(atoi(tokens[k+1].c_str()));
                you.redraw_stats.init(true);
                you.redraw_evasion = true;
                return;
            }
        }
    }
    if (size > 5 && tokens[0] == "Gold")
    {
        for (int k = 1; k < size; k++)
        {
            if (tokens[k] == "Dex")
            {
                you.base_stats[STAT_DEX] = debug_cap_stat(atoi(tokens[k+1].c_str()));
                you.redraw_stats.init(true);
                you.redraw_evasion = true;
                return;
            }
        }
    }

    return;
}

void wizard_load_dump_file()
{
    char filename[80];
    msgwin_get_line_autohist("Which dump file? ", filename, sizeof(filename));
    if (filename[0] == '\0')
    {
        canned_msg(MSG_OK);
        return;
    }

    you.init_skills();

    FileLineInput f(filename);
    while (!f.eof())
        _wizard_modify_character(f.get_line());

    init_skill_order();
    init_can_train();
    init_train();
    init_training();
}

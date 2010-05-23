/*
 *  File:       wiz-you.cc
 *  Summary:    Player related debugging functions.
 *  Written by: Linley Henzell and Jesse Jones
 */

#include "AppHdr.h"

#include "wiz-you.h"

#include "cio.h"
#include "debug.h"
#include "dbg-util.h"
#include "food.h"
#include "godprayer.h"
#include "libutil.h"
#include "message.h"
#include "mutation.h"
#include "newgame.h"
#include "ng-setup.h"
#include "output.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "xom.h"

#ifdef WIZARD
void wizard_change_species( void )
{
    char specs[80];
    int i;

    msgwin_get_line("What species would you like to be now? " ,
                    specs, sizeof(specs));

    if (specs[0] == '\0')
        return;
    strlwr(specs);

    species_type sp = SP_UNKNOWN;

    for (i = 0; i < NUM_SPECIES; ++i)
    {
        const species_type si = static_cast<species_type>(i);
        const std::string sp_name =
            lowercase_string(species_name(si, you.experience_level));

        std::string::size_type pos = sp_name.find(specs);
        if (pos != std::string::npos)
        {
            if (pos == 0 && *specs)
            {
                // We prefer prefixes over partial matches.
                sp = si;
                break;
            }
            else
                sp = si;
        }
    }

    if (sp == SP_UNKNOWN)
    {
        mpr("That species isn't available.");
        return;
    }

    // Re-scale skill-points.
    for (i = 0; i < NUM_SKILLS; ++i)
    {
        you.skill_points[i] *= species_skills( i, sp );
        you.skill_points[i] /= species_skills( i, you.species );
    }

    you.species = sp;
    you.is_undead = get_undead_state(sp);

    // Change permanent mutations, but preserve non-permanent ones.
    unsigned char prev_muts[NUM_MUTATIONS];
    for (i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (you.demon_pow[i] > 0)
        {
            if (you.demon_pow[i] > you.mutation[i])
                you.mutation[i] = 0;
            else
                you.mutation[i] -= you.demon_pow[i];

            you.demon_pow[i] = 0;
        }
        prev_muts[i] = you.mutation[i];
    }
    give_basic_mutations(sp);
    for (i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (prev_muts[i] > you.demon_pow[i])
            you.demon_pow[i] = 0;
        else
            you.demon_pow[i] -= prev_muts[i];
    }

    switch (sp)
    {
    case SP_GREEN_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_POISON_RESISTANCE, 1);
        break;

    case SP_RED_DRACONIAN:
        if (you.experience_level >= 14)
            perma_mutate(MUT_HEAT_RESISTANCE, 1);
        break;

    case SP_WHITE_DRACONIAN:
        if (you.experience_level >= 14)
            perma_mutate(MUT_COLD_RESISTANCE, 1);
        break;


    case SP_BLACK_DRACONIAN:
        if (you.experience_level >= 18)
            perma_mutate(MUT_SHOCK_RESISTANCE, 1);
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
            ++you.demon_pow[m];
        }
        break;
    }

    default:
        break;
    }

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
    if (cancelable_get_line_autohist( specs, sizeof( specs ) )
        || specs[0] == '\0')
    {
        canned_msg( MSG_OK );
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

    if (your_spells( static_cast<spell_type>(spell), 0, false )
                == SPRET_ABORT)
    {
        crawl_state.cancel_cmd_repeat();
    }
}
#endif

void wizard_heal(bool super_heal)
{
    if (super_heal)
    {
        // Clear more stuff and give a HP boost.
        you.magic_contamination = 0;
        you.duration[DUR_LIQUID_FLAMES] = 0;
        you.clear_beholders();
        inc_hp(10, true);
    }

    // Clear most status ailments.
    you.rotting = 0;
    you.disease = 0;
    you.duration[DUR_CONF]      = 0;
    you.duration[DUR_MISLED]    = 0;
    you.duration[DUR_POISONING] = 0;
    set_hp(you.hp_max, false);
    set_mp(you.max_magic_points, false);
    set_hunger(10999, true);
    you.redraw_hit_points = true;
}

void wizard_set_hunger_state()
{
    std::string hunger_prompt =
        "Set hunger state to s(T)arving, (N)ear starving, (H)ungry";
    if (you.species == SP_GHOUL)
        hunger_prompt += " or (S)atiated";
    else
        hunger_prompt += ", (S)atiated, (F)ull or (E)ngorged";
    hunger_prompt += "? ";

    mprf(MSGCH_PROMPT, "%s", hunger_prompt.c_str());

    const int c = tolower(getch());

    // Values taken from food.cc.
    switch (c)
    {
    case 't': you.hunger = 500;   break;
    case 'n': you.hunger = 1200;  break;
    case 'h': you.hunger = 2400;  break;
    case 's': you.hunger = 5000;  break;
    case 'f': you.hunger = 8000;  break;
    case 'e': you.hunger = 12000; break;
    default:  canned_msg(MSG_OK); break;
    }

    food_change();

    if (you.species == SP_GHOUL && you.hunger_state >= HS_SATIATED)
        mpr("Ghouls can never be full or above!");
}

void wizard_set_piety()
{
    if (you.religion == GOD_NO_GOD)
    {
        mpr("You are not religious!");
        return;
    }

    mprf(MSGCH_PROMPT, "Enter new piety value (current = %d, Enter for 0): ",
         you.piety);
    char buf[30];
    if (cancelable_get_line_autohist(buf, sizeof buf))
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

    if (you.religion == GOD_XOM)
    {
        you.piety = newpiety;

        // For Xom, also allow setting interest.
        mprf(MSGCH_PROMPT, "Enter new interest (current = %d, Enter for 0): ",
             you.gift_timeout);

        if (cancelable_get_line_autohist(buf, sizeof buf))
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

        const std::string new_xom_favour = describe_xom_favour();
        const std::string msg = "You are now " + new_xom_favour;
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
    int diff = newpiety - you.piety;
    if (diff > 0)
        gain_piety(diff);
    else
        lose_piety(-diff);

    // Automatically reduce penance to 0.
    if (you.penance[you.religion] > 0)
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
    int skill = debug_prompt_for_skill( "Which skill (by name)? " );

    if (skill == -1)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr("Exercising...");
        exercise(skill, 100);
    }
}
#endif

#ifdef WIZARD
void wizard_set_skill_level(void)
{
    int skill = debug_prompt_for_skill( "Which skill (by name)? " );

    if (skill == -1)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr(skill_name(skill));
        int amount = debug_prompt_for_int( "To what level? ", true );

        if (amount < 0)
            canned_msg( MSG_OK );
        else
        {
            const int old_amount = you.skills[skill];
            const int points = (skill_exp_needed( amount )
                                * species_skills( skill, you.species )) / 100;

            you.skill_points[skill] = points + 1;
            you.skills[skill] = amount;

            calc_total_skill_points();

            redraw_skill(you.your_name, player_title());

            switch (skill)
            {
            case SK_FIGHTING:
                calc_hp();
                break;

            case SK_SPELLCASTING:
            case SK_INVOCATIONS:
            case SK_EVOCATIONS:
                calc_mp();
                break;

            case SK_DODGING:
                you.redraw_evasion = true;
                break;

            case SK_ARMOUR:
                you.redraw_armour_class = true;
                you.redraw_evasion = true;
                break;

            default:
                break;
            }

            mprf("%s %s to skill level %d.",
                 (old_amount < amount ? "Increased" :
                  old_amount > amount ? "Lowered"
                                      : "Reset"),
                 skill_name(skill), amount);

            if (skill == SK_STEALTH && amount == 27)
            {
                mpr("If you set the stealth skill to a value higher than 27, "
                    "hide mode is activated, and monsters won't notice you.");
            }
        }
    }
}
#endif


#ifdef WIZARD
void wizard_set_all_skills(void)
{
    int i;
    int amount =
            debug_prompt_for_int( "Set all skills to what level? ", true );

    if (amount < 0)             // cancel returns -1 -- bwr
        canned_msg( MSG_OK );
    else
    {
        if (amount > 27)
            amount = 27;

        for (i = SK_FIGHTING; i < NUM_SKILLS; ++i)
        {
            if (is_invalid_skill(i))
                continue;

            const int points = (skill_exp_needed( amount )
                                * species_skills( i, you.species )) / 100;

            you.skill_points[i] = points + 1;
            you.skills[i] = amount;
        }

        redraw_skill(you.your_name, player_title());

        calc_total_skill_points();

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
            msg = "You are immune to mutations, remove immunity?";
        else
            msg = "You are resistant to mutations, remove resistance?";

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
        return (false);
    }
    const bool force = (answer == 1);

    if (player_mutation_level(MUT_MUTATION_RESISTANCE) == 3 && !force)
    {
        mpr("Can't mutate when immune to mutations without forcing it.");
        crawl_state.cancel_cmd_repeat();
        return (false);
    }

    answer = yesnoquit("Treat mutation as god gift?", true, 'n');
    if (answer == -1)
    {
        canned_msg(MSG_OK);
        return (false);
    }
    const bool god_gift = (answer == 1);

    msgwin_get_line("Which mutation (name, 'good', 'bad', 'any', 'xom')? ",
                    specs, sizeof(specs));

    if (specs[0] == '\0')
        return (false);

    strlwr(specs);

    mutation_type mutat = NUM_MUTATIONS;

    if (strcmp(specs, "good") == 0)
        mutat = RANDOM_GOOD_MUTATION;
    else if (strcmp(specs, "bad") == 0)
        mutat = RANDOM_BAD_MUTATION;
    else if (strcmp(specs, "any") == 0)
        mutat = RANDOM_MUTATION;
    else if (strcmp(specs, "xom") == 0)
        mutat = RANDOM_XOM_MUTATION;

    if (mutat != NUM_MUTATIONS)
    {
        int old_resist = player_mutation_level(MUT_MUTATION_RESISTANCE);

        success = mutate(mutat, true, force, god_gift);

        if (old_resist < player_mutation_level(MUT_MUTATION_RESISTANCE)
            && !force)
        {
            crawl_state.cancel_cmd_repeat("Your mutation resistance has "
                                          "increased.");
        }
        return (success);
    }

    std::vector<mutation_type> partial_matches;

    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        if (!is_valid_mutation(mut))
            continue;

        const mutation_def& mdef = get_mutation_def(mut);
        if (strcmp(specs, mdef.wizname) == 0)
        {
            mutat = mut;
            break;
        }

        if (strstr(mdef.wizname, specs))
            partial_matches.push_back(mut);
    }

    // If only one matching mutation, use that.
    if (mutat == NUM_MUTATIONS && partial_matches.size() == 1)
        mutat = partial_matches[0];

    if (mutat == NUM_MUTATIONS)
    {
        crawl_state.cancel_cmd_repeat();

        if (partial_matches.size() == 0)
            mpr("No matching mutation names.");
        else
        {
            std::vector<std::string> matches;

            for (unsigned int i = 0; i < partial_matches.size(); ++i)
                matches.push_back(get_mutation_def(partial_matches[i]).wizname);

            std::string prefix = "No exact match for mutation '" +
                std::string(specs) +  "', possible matches are: ";

            // Use mpr_comma_separated_list() because the list
            // might be *LONG*.
            mpr_comma_separated_list(prefix, matches, " and ", ", ",
                                     MSGCH_DIAGNOSTICS);
        }

        return (false);
    }
    else
    {
        mprf("Found #%d: %s (\"%s\")", (int) mutat,
             get_mutation_def(mutat).wizname,
             mutation_name(mutat, 1, false).c_str());

        const int levels =
            debug_prompt_for_int("How many levels to increase or decrease? ",
                                  false);

        if (levels == 0)
        {
            canned_msg(MSG_OK);
            success = false;
        }
        else if (levels > 0)
        {
            for (int i = 0; i < levels; ++i)
                if (mutate(mutat, true, force, god_gift))
                    success = true;
        }
        else
        {
            for (int i = 0; i < -levels; ++i)
                if (delete_mutation(mutat, true, force, god_gift))
                    success = true;
        }
    }

    return (success);
}
#endif

#ifdef WIZARD
void wizard_get_religion(void)
{
    char specs[80];

    msgwin_get_line("Which god (by name)? ", specs, sizeof(specs));

    if (specs[0] == '\0')
        return;

    strlwr(specs);

    god_type god = GOD_NO_GOD;

    for (int i = 1; i < NUM_GODS; ++i)
    {
        const god_type gi = static_cast<god_type>(i);
        if (lowercase_string(god_name(gi)).find(specs) != std::string::npos)
        {
            god = gi;
            break;
        }
    }

    if (god == GOD_NO_GOD)
        mpr("That god doesn't seem to be taking followers today.");
    else
    {
        dungeon_feature_type feat =
            static_cast<dungeon_feature_type>( DNGN_ALTAR_FIRST_GOD + god - 1 );
        dungeon_terrain_changed(you.pos(), feat, false);

        pray();
    }
}
#endif

void wizard_set_stats()
{
    char buf[80];
    mprf(MSGCH_PROMPT, "Enter values for Str, Int, Dex (space separated): ");
    if (cancelable_get_line_autohist(buf, sizeof buf))
        return;

    int sstr = you.strength(),
        sdex = you.dex(),
        sint = you.intel();

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
    "levitation",
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
    "stonemail",
    "controlled flight",
    "teleport",
    "control teleport",
    "breath weapon",
    "transformation",
    "death channel",
    "deflect missiles",
    "phase shift",
    "see invisible",
    "weapon brand",
    "demonic guardian",
    "pbd",
    "silence",
    "condensation shield",
    "stoneskin",
    "gourmand",
    "bargain",
    "insulation",
    "resist poison",
    "resist fire",
    "resist cold",
    "slaying",
    "stealth",
    "magic shield",
    "sleep",
    "sage",
    "telepathy",
    "petrified",
    "lowered mr",
    "repel stairs move",
    "repel stairs climb",
    "slimify",
    "time step",
    "icemail depleted",
    "misled"
};

void wizard_edit_durations( void )
{
    COMPILE_CHECK(ARRAYSZ(dur_names) == NUM_DURATIONS, dur_names_size);
    std::vector<int> durs;
    size_t max_len = 0;

    for (int i = 0; i < NUM_DURATIONS; ++i)
    {
        if (!you.duration[i])
            continue;

        max_len = std::max(strlen(dur_names[i]), max_len);
        durs.push_back(i);
    }

    if (durs.size() > 0)
    {
        for (unsigned int i = 0; i < durs.size(); ++i)
        {
            int dur = durs[i];
            mprf(MSGCH_PROMPT, "%c) %-*s : %d", 'a' + i, max_len,
                 dur_names[dur], you.duration[dur]);
        }
        mpr("", MSGCH_PROMPT);
        mpr("Edit which duration (letter or name)? ", MSGCH_PROMPT);
    }
    else
        mpr("Edit which duration (name)? ", MSGCH_PROMPT);

    char buf[80];

    if (cancelable_get_line_autohist(buf, sizeof buf) || strlen(buf) == 0)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (!strlcpy(buf, lowercase_string(trimmed_string(buf)).c_str(), sizeof(buf)))
    {
        canned_msg( MSG_OK );
        return;
    }

    int choice = -1;

    if (strlen(buf) == 1)
    {
        if (durs.size() == 0)
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
        std::vector<int>         matches;
        std::vector<std::string> match_names;
        max_len = 0;

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
        else if (matches.size() == 0)
        {
            mprf(MSGCH_PROMPT, "No durations matching '%s'.", buf);
            return;
        }
        else
        {
            std::string prefix = "No exact match for duration '";
            prefix += buf;
            prefix += "', possible matches are: ";

            mpr_comma_separated_list(prefix, match_names, " and ", ", ",
                                     MSGCH_DIAGNOSTICS);
            return;
        }
    }

    snprintf(buf, sizeof(buf), "Set '%s' to: ", dur_names[choice]);
    int num = debug_prompt_for_int(buf, false);

    if (num == 0)
    {
        mpr("Can't set duration directly to 0, setting it to 1 instead.",
            MSGCH_PROMPT);
        num = 1;
    }
    you.duration[choice] = num;
}

static void debug_uptick_xl(int newxl)
{
    while (newxl > you.experience_level)
    {
        you.experience = 1 + exp_needed( 2 + you.experience_level );
        level_change(true);
    }
}

static void debug_downtick_xl(int newxl)
{
    you.hp = you.hp_max;
    while (newxl < you.experience_level)
    {
        // Each lose_level() subtracts 4 HP, so do this to avoid death
        // and/or negative HP when going from a high level to a low level.
        you.hp     = std::max(5, you.hp);
        you.hp_max = std::max(5, you.hp_max);

        lose_level();
    }

    you.hp       = std::max(1, you.hp);
    you.hp_max   = std::max(1, you.hp_max);

    you.base_hp  = std::max(5000,              you.base_hp);
    you.base_hp2 = std::max(5000 + you.hp_max, you.base_hp2);
}

void wizard_set_xl()
{
    mprf(MSGCH_PROMPT, "Enter new experience level: ");
    char buf[30];
    if (cancelable_get_line_autohist(buf, sizeof buf))
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

    no_messages mx;
    if (newxl < you.experience_level)
        debug_downtick_xl(newxl);
    else
        debug_uptick_xl(newxl);
}

void wizard_get_god_gift (void)
{
    if (you.religion == GOD_NO_GOD)
    {
        mpr("You are not religious!");
        return;
    }

    if (!do_god_gift(false, true))
        mpr("Nothing happens.");
}

/**
 * @file
 * @brief Functions used to print information about gods.
 **/

#include "AppHdr.h"

#include "describe-god.h"

#include <iomanip>

#include "ability.h"
#include "branch.h"
#include "cio.h"
#include "database.h"
#include "describe.h"
#include "godabil.h"
#include "godpassive.h"
#include "godprayer.h" // can_do_capstone_ability()
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "player.h"
#include "religion.h"
#include "skills2.h"
#include "spl-util.h"
#include "stringutil.h"
#include "unicode.h"
#include "xom.h"

extern ability_type god_abilities[NUM_GODS][MAX_GOD_ABILITIES];

static bool _print_final_god_abil_desc(int god, const string &final_msg,
                                       const ability_type abil)
{
    // If no message then no power.
    if (final_msg.empty())
        return false;

    string buf = final_msg;

    // For ability slots that give more than one ability, display
    // "Various" instead of the cost of the first ability.
    const string cost =
    "(" +
    ((abil == ABIL_ELYVILON_LESSER_HEALING_SELF
      || abil == ABIL_ELYVILON_GREATER_HEALING_OTHERS
      || abil == ABIL_YRED_RECALL_UNDEAD_SLAVES) ?
     "Various" : make_cost_description(abil))
    + ")";

    if (cost != "(None)")
    {
        // XXX: Handle the display better when the description and cost
        // are too long for the screen.
        buf = chop_string(buf, get_number_of_cols() - 1 - strwidth(cost));
        buf += cost;
    }

    cprintf("%s\n", buf.c_str());

    return true;
}

static bool _print_god_abil_desc(int god, int numpower)
{
    // Combine the two lesser healing powers together.
    if (god == GOD_ELYVILON && numpower == 0)
    {
        _print_final_god_abil_desc(god,
                                   "You can provide lesser healing for yourself"
                                   " and others.",
                                   ABIL_ELYVILON_LESSER_HEALING_SELF);
        return true;
    }
    const char* pmsg = god_gain_power_messages[god][numpower];

    // If no message then no power.
    if (!pmsg[0])
        return false;

    // Don't display ability upgrades here.
    string buf = adjust_abil_message(pmsg, false);
    if (buf.empty())
        return false;

    if (!isupper(pmsg[0])) // Complete sentence given?
        buf = "You can " + buf + ".";

    // This might be ABIL_NON_ABILITY for passive abilities.
    const ability_type abil = god_abilities[god][numpower];
    _print_final_god_abil_desc(god, buf, abil);

    return true;
}

static int _piety_level(int piety)
{
    return (piety >= piety_breakpoint(5)) ? 7 :
           (piety >= piety_breakpoint(4)) ? 6 :
           (piety >= piety_breakpoint(3)) ? 5 :
           (piety >= piety_breakpoint(2)) ? 4 :
           (piety >= piety_breakpoint(1)) ? 3 :
           (piety >= piety_breakpoint(0)) ? 2 :
           (piety >                    0) ? 1
                                          : 0;
}

static int _gold_level()
{
    return (you.gold >= 50000) ? 7 :
           (you.gold >= 10000) ? 6 :
           (you.gold >=  5000) ? 5 :
           (you.gold >=  1000) ? 4 :
           (you.gold >=   500) ? 3 :
           (you.gold >=   100) ? 2
                               : 1;
}

static string _describe_favour(god_type which_god)
{
    if (player_under_penance())
    {
        const int penance = you.penance[which_god];
        return (penance >= 50) ? "Godly wrath is upon you!" :
               (penance >= 20) ? "You've transgressed heavily! Be penitent!" :
               (penance >=  5) ? "You are under penance."
                               : "You should show more discipline.";
    }

    if (which_god == GOD_XOM)
        return uppercase_first(describe_xom_favour());

    const int rank = which_god == GOD_GOZAG ? _gold_level()
    : _piety_level(you.piety);

    const string godname = god_name(which_god);
    switch (rank)
    {
        case 7:  return "A prized avatar of " + godname;
        case 6:  return "A favoured servant of " + godname + ".";
        case 5:

            if (you_worship(GOD_DITHMENOS))
                return "A glorious shadow in the eyes of " + godname + ".";
            else
                return "A shining star in the eyes of " + godname + ".";

        case 4:

            if (you_worship(GOD_DITHMENOS))
                return "A rising shadow in the eyes of " + godname + ".";
            else
                return "A rising star in the eyes of " + godname + ".";

        case 3:  return uppercase_first(godname) + " is most pleased with you.";
        case 2:  return uppercase_first(godname) + " is pleased with you.";
        default: return uppercase_first(godname) + " is noncommittal.";
    }
}

static string _religion_help(god_type god)
{
    string result = "";

    switch (god)
    {
        case GOD_ZIN:
            if (can_do_capstone_ability(god))
            {
                result += "You can have all your mutations cured.\n";
            }
            result += "You can pray at an altar to donate money.";
            break;

        case GOD_SHINING_ONE:
        {
            const int halo_size = you.halo_radius2();
            if (halo_size >= 0)
            {
                if (!result.empty())
                    result += " ";

                result += "You radiate a ";

                if (halo_size > 37)
                    result += "large ";
                else if (halo_size > 10)
                    result += "";
                else
                    result += "small ";

                result += "righteous aura, and all beings within it are "
                          "easier to hit.";
            }
            if (can_do_capstone_ability(god))
            {
                if (!result.empty())
                    result += " ";

                result += "You can pray at an altar to have your weapon "
                          "blessed, especially a long blade or demon "
                          "weapon.";
            }
            break;
        }

        case GOD_ELYVILON:
            result += "You can pray to destroy weapons on the ground in "
            + apostrophise(god_name(god)) + " name. Inscribe them "
            "with !p, !* or =p to avoid sacrificing them accidentally.";
            break;

        case GOD_LUGONU:
            if (can_do_capstone_ability(god))
            {
                result += "You can pray at an altar to have your weapon "
                          "corrupted.";
            }
            break;

        case GOD_KIKUBAAQUDGHA:
            if (can_do_capstone_ability(god))
            {
                result += "You can pray at an altar to have your necromancy "
                          "enhanced.";
            }
            break;

        case GOD_BEOGH:
            result += "You can pray to sacrifice all orcish remains on your "
                      "square.";
            break;

        case GOD_FEDHAS:
            if (you.piety >= piety_breakpoint(0))
            {
                result += "Evolving plants requires fruit, and evolving "
                          "fungi requires piety.";
            }
            break;

        case GOD_GOZAG:
            if (can_do_capstone_ability(god))
            {
                result += "You can place a non-artefact item on an altar and "
                          "pray to duplicate that item.";
            }
            break;

        default:
            break;
    }

    if (god_likes_fresh_corpses(god))
    {
        if (!result.empty())
            result += " ";

        result += "You can pray to sacrifice all fresh corpses on your "
                  "square.";
    }

    return result;
}

// The various titles granted by the god of your choice.  Note that Xom
// doesn't use piety the same way as the other gods, so these are just
// placeholders.
static const char *divine_title[NUM_GODS][8] =
{
    // No god.
    {"Buglet",             "Firebug",               "Bogeybug",                 "Bugger",
        "Bugbear",            "Bugged One",            "Giant Bug",                "Lord of the Bugs"},

    // Zin.
    {"Blasphemer",         "Anchorite",             "Apologist",                "Pious",
        "Devout",             "Orthodox",              "Immaculate",               "Bringer of Law"},

    // The Shining One.
    {"Honourless",         "Acolyte",               "Righteous",                "Unflinching",
        "Holy Warrior",       "Exorcist",              "Demon Slayer",             "Bringer of Light"},

    // Kikubaaqudgha -- scholarly death.
    {"Tormented",          "Purveyor of Pain",      "Death's Scholar",          "Merchant of Misery",
        "Death's Artisan",    "Dealer of Despair",     "Black Sun",                "Lord of Darkness"},

    // Yredelemnul -- zombie death.
    {"Traitor",            "Zealot",                "Exhumer",                  "Fey @Genus@",
        "Soul Tainter",       "Sculptor of Flesh",     "Harbinger of Death",       "Grim Reaper"},

    // Xom.
    {"Toy",                "Toy",                   "Toy",                      "Toy",
        "Toy",                "Toy",                   "Toy",                      "Toy"},

    // Vehumet -- battle mage theme.
    {"Meek",               "Sorcerer's Apprentice", "Scholar of Destruction",   "Caster of Ruination",
        "Battle Magician",    "Warlock",               "Annihilator",              "Luminary of Lethal Lore"},

    // Okawaru -- battle theme.
    {"Coward",             "Struggler",             "Combatant",                "Warrior",
        "Knight",             "Warmonger",             "Commander",                "Victor of a Thousand Battles"},

    // Makhleb -- chaos theme.
    {"Orderly",            "Spawn of Chaos",        "Disciple of Annihilation", "Fanfare of Bloodshed",
        "Fiendish",           "Demolition @Genus@",    "Pandemonic",               "Champion of Chaos"},

    // Sif Muna -- scholarly theme.
    {"Ignorant",           "Disciple",              "Student",                  "Adept",
        "Scribe",             "Scholar",               "Sage",                     "Genius of the Arcane"},

    // Trog -- anger theme.
    {"Puny",               "Troglodyte",            "Angry Troglodyte",         "Frenzied",
        "@Genus@ of Prey",    "Rampant",               "Wild @Genus@",             "Bane of Scribes"},

    // Nemelex Xobeh -- alluding to Tarot and cards.
    {"Unlucky @Genus@",    "Pannier",               "Jester",                   "Fortune-Teller",
        "Soothsayer",         "Magus",                 "Cardsharp",                "Hand of Fortune"},

    // Elyvilon.
    {"Sinner",             "Comforter",             "Caregiver",                "Practitioner",
        "Pacifier",           "Purifying @Genus@",     "Faith Healer",             "Bringer of Life"},

    // Lugonu -- distortion theme.
    {"Pure",               "Abyss-Baptised",        "Unweaver",                 "Distorting @Genus@",
        "Agent of Entropy",   "Schismatic",            "Envoy of Void",            "Corrupter of Planes"},

    // Beogh -- messiah theme.
    {"Apostate",           "Messenger",             "Proselytiser",             "Priest",
        "Missionary",         "Evangelist",            "Apostle",                  "Messiah"},

    // Jiyva -- slime and jelly theme.
    {"Scum",               "Jelly",                 "Squelcher",                "Dissolver",
        "Putrid Slime",       "Consuming @Genus@",     "Archjelly",                "Royal Jelly"},

    // Fedhas Madash -- nature theme.
    {"@Walking@ Fertiliser", "Green @Genus@",       "Inciter",                  "Photosynthesist",
        "Cultivator",         "Green Death",           "Nimbus",                   "Force of Nature"},

    // Cheibriados -- slow theme
    {"Unwound @Genus@",    "Timekeeper",            "Righteous Timekeeper",     "Chronographer",
        "Splendid Chronographer", "Chronicler",        "Eternal Chronicler",       "Ticktocktomancer"},

    // Ashenzari -- divination theme
    {"Star-crossed",       "Cursed",                "Initiated",                "Seer",
        "Soothsayer",         "Oracle",                "Illuminatus",              "Omniscient"},

    // Dithmenos -- darkness theme
    {"Illuminated",        "Gloomy",                "Aphotic",                  "Caliginous",
        "Darkened",           "Shadowed",              "Eclipsing",                "Eternal Night"},

    // Gozag -- entrepreneur theme
    {"Profligate",         "Pauper",                "Entrepreneur",             "Capitalist",
        "Rich",               "Opulent",               "Tycoon",                   "Plutocrat"},

    // Qazlal -- natural disaster theme
    {"Unspoiled",          "@Adj@ Mishap",          "Lightning Rod",            "@Adj@ Disaster",
        "Eye of the Storm",   "@Adj@ Catastrophe",     "@Adj@ Cataclysm",          "End of an Era"},

    // Ru -- enlightenment theme
    {"Sleeper",           "Questioner",             "Initiate",                 "Seeker of Truth",
        "Walker of the Path","Lifter of the Veil",     "Drinker of Unreality",     "Transcendent"},
};

string god_title(god_type which_god, species_type which_species, int piety)
{
    string title;
    if (player_under_penance(which_god))
        title = divine_title[which_god][0];
    else if (which_god == GOD_GOZAG)
        title = divine_title[which_god][_gold_level()];
    else
        title = divine_title[which_god][_piety_level(piety)];

    //XXX: unify with stuff in skills2.cc
    title = replace_all(title, "@Genus@", species_name(which_species, true, false));
    title = replace_all(title, "@Adj@", species_name(which_species, false, true));
    title = replace_all(title, "@Walking@", (species_walking_verb(which_species) + "ing"));

    return title;
}

static string _describe_ash_skill_boost()
{
    if (!you.bondage_level)
    {
        return "Ashenzari won't support your skills until you bind yourself "
               "with cursed items.";
    }

    static const char* bondage_parts[NUM_ET] = { "Weapon hand", "Shield hand",
                                                 "Armour", "Jewellery" };
    static const char* bonus_level[3] = { "Low", "Medium", "High" };
    ostringstream desc;
    desc.setf(ios::left);
    desc << setw(18) << "Bound part";
    desc << setw(30) << "Boosted skills";
    desc << "Bonus\n";

    for (int i = ET_WEAPON; i < NUM_ET; i++)
    {
        if (you.bondage[i] <= 0 || i == ET_SHIELD && you.bondage[i] == 3)
            continue;

        desc << setw(18);
        if (i == ET_WEAPON && you.bondage[i] == 3)
            desc << "Hands";
        else
            desc << bondage_parts[i];

        string skills;
        map<skill_type, int8_t> boosted_skills = ash_get_boosted_skills(eq_type(i));
        const int8_t bonus = boosted_skills.begin()->second;
        map<skill_type, int8_t>::iterator it = boosted_skills.begin();

        // First, we keep only one magic school skill (conjuration).
        // No need to list all of them since we boost all or none.
        while (it != boosted_skills.end())
        {
            if (it->first > SK_CONJURATIONS && it->first <= SK_LAST_MAGIC)
            {
                boosted_skills.erase(it);
                it = boosted_skills.begin();
            }
            else
                ++it;
        }

        it = boosted_skills.begin();
        while (!boosted_skills.empty())
        {
            // For now, all the bonuses from the same bounded part have
            // the same level.
            ASSERT(bonus == it->second);
            if (it->first == SK_CONJURATIONS)
                skills += "Magic schools";
            else
                skills += skill_name(it->first);

            if (boosted_skills.size() > 2)
                skills += ", ";
            else if (boosted_skills.size() == 2)
                skills += " and ";

            boosted_skills.erase(it);
            ++it;
        }

        desc << setw(30) << skills;
        desc << bonus_level[bonus -1] << "\n";
    }

    return desc.str();
}

string get_skill_description(skill_type skill, bool need_title)
{
    string lookup = skill_name(skill);
    string result = "";

    if (need_title)
    {
        result = lookup;
        result += "\n\n";
    }

    result += getLongDescription(lookup);

    switch (skill)
    {
        case SK_INVOCATIONS:
            if (you.species == SP_DEMIGOD)
            {
                result += "\n";
                result += "How on earth did you manage to pick this up?";
            }
            else if (you_worship(GOD_TROG))
            {
                result += "\n";
                result += "Note that Trog doesn't use Invocations, due to its "
                          "close connection to magic.";
            }
            else if (you_worship(GOD_NEMELEX_XOBEH))
            {
                result += "\n";
                result += "Note that Nemelex uses Evocations rather than "
                          "Invocations.";
            }
            break;

        case SK_EVOCATIONS:
            if (you_worship(GOD_NEMELEX_XOBEH))
            {
                result += "\n";
                result += "This is the skill all of Nemelex's abilities rely "
                          "on.";
            }
            break;

        case SK_SPELLCASTING:
            if (you_worship(GOD_TROG))
            {
                result += "\n";
                result += "Keep in mind, though, that Trog will greatly "
                          "disapprove of this.";
            }
            break;
        default:
            // No further information.
            break;
    }

    return result;
}

// from dgn-overview.cc
extern map<branch_type, set<level_id> > stair_level;

// XXX: apply padding programmatically?
static const char* const bribe_susceptibility_adjectives[] =
{
    "none       ",
    "very low   ",
    "low        ",
    "moderate   ",
    "high       ",
    "very high  "
};

/**
 * Populate a provided vector with a list of bribable branches which are known
 * to the player.
 *
 * @param[out] targets      A list of bribable branches.
 */
static void _list_bribable_branches(vector<branch_type> &targets)
{
    for (branch_iterator it; it; ++it)
    {
        const branch_type br = it->id;
        if (!gozag_branch_bribable(br))
            continue;

        // If you don't know the branch exists, don't list it;
        // this mainly plugs info leaks about Lair branch structure.
        if (!stair_level.count(br) && is_random_subbranch(br))
            continue;

        targets.push_back(br);
    }
}

/**
 * Describe the current options for Gozag's bribe branch ability.
 *
 * @return      A description of branches' bribe status.
 */
static string _describe_branch_bribability()
{
    string ret = "You can bribe the following branches:\n";
    vector<branch_type> targets;
    _list_bribable_branches(targets);

    size_t width = 0;
    for (unsigned int i = 0; i < targets.size(); i++)
        width = max(width, strlen(branches[targets[i]].longname));

    for (unsigned int i = 0; i < targets.size(); i++)
    {
        string line(branches[targets[i]].longname);
        line += string(width + 1 - strwidth(line), ' ');
        // XXX: move this elsewhere?
        switch (targets[i])
        {
            case BRANCH_ORC:
                line += "(orcs)              ";
                break;
            case BRANCH_ELF:
                line += "(elves)             ";
                break;
            case BRANCH_SNAKE:
                line += "(nagas/salamanders) ";
                break;
            case BRANCH_SHOALS:
                line += "(merfolk)           ";
                break;
            case BRANCH_VAULTS:
                line += "(humans)            ";
                break;
            case BRANCH_ZOT:
                line += "(draconians)        ";
                break;
            case BRANCH_COCYTUS:
            case BRANCH_DIS:
            case BRANCH_GEHENNA:
            case BRANCH_TARTARUS:
                line += "(demons)            ";
                break;
            default:
                line += "(buggy)             ";
                break;
        }

        line += "Susceptibility: ";
        const int suscept = gozag_branch_bribe_susceptibility(targets[i]);
        ASSERT(suscept >= 0
               && suscept < ARRAYSZ(bribe_susceptibility_adjectives));
        line += bribe_susceptibility_adjectives[suscept];

        if (!branch_bribe[targets[i]])
            line += "not bribed";
        else
            line += make_stringf("$%d", branch_bribe[targets[i]]);

        ret += line + "\n";
    }

    return ret;
}

/**
 * Print a guide to toggling between description screens, and check if the
 * player does so.
 *
 * @return Whether the player chose to toggle to the other description screen.
 */
static bool _check_description_toggle()
{
    const int bottom_line = min(30, get_number_of_lines());

    cgotoxy(1, bottom_line);
    formatted_string::parse_string(
                                   "Press '<w>!</w>' or '<w>^</w>'"
#ifdef USE_TILE_LOCAL
                                   " or <w>Right-click</w>"
#endif
                                   " to toggle between the overview and the"
                                   " detailed description.").display();

    mouse_control mc(MOUSE_MODE_MORE);

    const int keyin = getchm();
    return keyin == '!' || keyin == CK_MOUSE_CMD || keyin == '^';

}

static void _detailed_god_description(god_type which_god)
{
    clrscr();

    const int width = min(80, get_number_of_cols());

    const string godname = uppercase_first(god_name(which_god, true));
    const int len = get_number_of_cols() - strwidth(godname);
    textcolor(god_colour(which_god));
    cprintf("%s%s\n", string(len / 2, ' ').c_str(), godname.c_str());
    textcolor(LIGHTGREY);
    cprintf("\n");

    string broken = get_god_powers(which_god);
    if (!broken.empty())
    {
        linebreak_string(broken, width);
        display_tagged_block(broken);
        cprintf("\n");
        cprintf("\n");
    }

    if (which_god != GOD_XOM)
    {
        broken = get_god_likes(which_god, true);
        linebreak_string(broken, width);
        display_tagged_block(broken);

        broken = get_god_dislikes(which_god, true);
        if (!broken.empty())
        {
            cprintf("\n");
            cprintf("\n");
            linebreak_string(broken, width);
            display_tagged_block(broken);
        }
        // Some special handling.
        broken = "";
        switch (which_god)
        {
            case GOD_ASHENZARI:
            case GOD_JIYVA:
            case GOD_TROG:
                broken = "Note that " + god_name(which_god) + " does not demand "
                "training of the Invocations skill. All abilities are "
                "purely based on piety.";

                if (which_god == GOD_ASHENZARI
                    && which_god == you.religion
                    && piety_rank() > 1)
                {
                    broken += "\n\n";
                    broken += _describe_ash_skill_boost();
                }
                break;

            case GOD_KIKUBAAQUDGHA:
                broken = "The power of Kikubaaqudgha's abilities is governed by "
                "Necromancy skill instead of Invocations.";
                break;

            case GOD_ELYVILON:
                broken = "Healing hostile monsters may pacify them, turning them "
                "neutral. Pacification works best on natural beasts, "
                "worse on monsters of your species, worse on other "
                "species, worst of all on demons and undead, and not at "
                "all on sleeping or mindless monsters. If it succeeds, "
                "you gain half of the monster's experience value and "
                "possibly some piety. Pacified monsters try to leave the "
                "level.";
                break;

            case GOD_NEMELEX_XOBEH:
                if (which_god == you.religion)
                {
                    broken = "The power of Nemelex Xobeh's abilities and of the "
                    "cards' effects is governed by Evocations skill "
                    "instead of Invocations.";
                }
                break;

            case GOD_GOZAG:
                broken = _describe_branch_bribability();
                break;

            default:
                break;
        }

        if (!broken.empty())
        {
            cprintf("\n");
            cprintf("\n");
            linebreak_string(broken, width);
            display_tagged_block(broken);
        }
    }

    if (_check_description_toggle())
        describe_god(which_god, true);
}

/**
 * Describe the given god's level of irritation at the player.
 *
 * Player may or may not be currently under penance.
 *
 * @param which_god     The god in question.
 * @return              A description of the god's ire (or lack thereof).
 */
static string _god_penance_message(god_type which_god)
{
    int which_god_penance = you.penance[which_god];

    // Give more appropriate message for the good gods.
    // XXX: ^ this is a hack
    if (which_god_penance > 0 && is_good_god(which_god))
    {
        if (is_good_god(you.religion))
            which_god_penance = 0;
        else if (!god_hates_your_god(which_god) && which_god_penance >= 5)
            which_god_penance = 2; // == "Come back to the one true church!"
    }

    const string penance_message =
        (which_god == GOD_NEMELEX_XOBEH
         && which_god_penance > 0 && which_god_penance <= 100)
            ? "%s doesn't play fair with you." :
        (which_god_penance >= 50)   ? "%s's wrath is upon you!" :
        (which_god_penance >= 20)   ? "%s is annoyed with you." :
        (which_god_penance >=  5)   ? "%s well remembers your sins." :
        (which_god_penance >   0)   ? "%s is ready to forgive your sins." :
        (you.worshipped[which_god]) ? "%s is ambivalent towards you."
                                    : "%s is neutral towards you.";

    return make_stringf(penance_message.c_str(),
                        uppercase_first(god_name(which_god)).c_str());
}

/**
 * Print a description of the powers & abilities currently granted to the
 * player by the given god.
 *
 * @param which_god     The god in question.
 */
static void _describe_god_powers(god_type which_god, int numcols)
{
    textcolor(LIGHTGREY);
    const char *header = "Granted powers:";
    const char *cost   = "(Cost)";
    cprintf("\n\n%s%*s%s\n", header,
            get_number_of_cols() - 1 - strwidth(header) - strwidth(cost),
            "", cost);
    textcolor(god_colour(which_god));

    // mv: Some gods can protect you from harm.
    // The god isn't really protecting the player - only sometimes saving
    // his life.
    bool have_any = false;

    if (god_can_protect_from_harm(which_god))
    {
        have_any = true;

        int prot_chance = 10 + you.piety/10; // chance * 100
        const char *when = "";

        switch (elyvilon_lifesaving())
        {
            case 1:
                when = ", especially when called upon";
                prot_chance += 100 - 3000/you.piety;
                break;
            case 2:
                when = ", and always does so when called upon";
                prot_chance = 100;
        }

        const char *how = (prot_chance >= 85) ? "carefully" :
                          (prot_chance >= 55) ? "often" :
                          (prot_chance >= 25) ? "sometimes"
                                              : "occasionally";

        string buf = uppercase_first(god_name(which_god));
        buf += " ";
        buf += how;
        buf += " watches over you";
        buf += when;
        buf += ".";

        _print_final_god_abil_desc(which_god, buf, ABIL_NON_ABILITY);
    }

    if (which_god == GOD_ZIN)
    {
        have_any = true;
        const char *how =
        (you.piety >= piety_breakpoint(5)) ? "carefully" :
        (you.piety >= piety_breakpoint(3)) ? "often" :
        (you.piety >= piety_breakpoint(1)) ? "sometimes" :
                                             "occasionally";

        cprintf("%s %s shields you from chaos.\n",
                uppercase_first(god_name(which_god)).c_str(), how);
    }
    else if (which_god == GOD_SHINING_ONE)
    {
        if (you.piety >= piety_breakpoint(1))
        {
            have_any = true;
            const char *how =
            (you.piety >= piety_breakpoint(5)) ? "carefully" :
            (you.piety >= piety_breakpoint(3)) ? "often" :
                                                 "sometimes";

            cprintf("%s %s shields you from negative energy.\n",
                    uppercase_first(god_name(which_god)).c_str(), how);
        }
    }
    else if (which_god == GOD_TROG)
    {
        have_any = true;
        string buf = "You can call upon "
        + god_name(which_god)
        + " to burn spellbooks in your surroundings.";
        _print_final_god_abil_desc(which_god, buf,
                                   ABIL_TROG_BURN_SPELLBOOKS);
    }
    else if (which_god == GOD_JIYVA)
    {
        if (you.piety >= piety_breakpoint(2))
        {
            have_any = true;
            cprintf("%s shields you from corrosive effects.\n",
                    uppercase_first(god_name(which_god)).c_str());
        }
        if (you.piety >= piety_breakpoint(1))
        {
            have_any = true;
            string buf = "You gain nutrition";
            if (you.piety >= piety_breakpoint(4))
                buf += ", power and health";
            else if (you.piety >= piety_breakpoint(3))
                buf += " and power";
            buf += " when your fellow slimes consume items.\n";
            _print_final_god_abil_desc(which_god, buf,
                                       ABIL_NON_ABILITY);
        }
    }
    else if (which_god == GOD_FEDHAS)
    {
        have_any = true;
        _print_final_god_abil_desc(which_god,
                                   "You can pray to speed up decomposition.",
                                   ABIL_NON_ABILITY);
        _print_final_god_abil_desc(which_god,
                                   "You can walk through plants and "
                                   "fire through allied plants.",
                                   ABIL_NON_ABILITY);
    }
    else if (which_god == GOD_ASHENZARI)
    {
        have_any = true;
        _print_final_god_abil_desc(which_god,
                                   "You are provided with a bounty of information.",
                                   ABIL_NON_ABILITY);
        _print_final_god_abil_desc(which_god,
                                   "You can pray to corrupt scrolls of remove curse on your square.",
                                   ABIL_NON_ABILITY);
    }
    else if (which_god == GOD_CHEIBRIADOS)
    {
        if (you.piety >= piety_breakpoint(0))
        {
            have_any = true;
            _print_final_god_abil_desc(which_god,
                                       uppercase_first(god_name(which_god))
                                       + " slows and strengthens your metabolism.",
                                       ABIL_NON_ABILITY);
        }
    }
    else if (which_god == GOD_ELYVILON)
    {
        // Only print this here if we don't have lesser self-healing.
        if (you.piety < piety_breakpoint(0) || player_under_penance())
        {
            have_any = true;
            _print_final_god_abil_desc(which_god,
                                       "You can provide lesser healing for others.",
                                       ABIL_ELYVILON_LESSER_HEALING_OTHERS);
        }
    }
    else if (which_god == GOD_VEHUMET)
    {
        set<spell_type>::iterator it = you.vehumet_gifts.begin();
        if (it != you.vehumet_gifts.end())
        {
            have_any = true;

            string offer = spell_title(*it);
            // If we have multiple offers, just summarise.
            if (++it != you.vehumet_gifts.end())
                offer = "some of Vehumet's most lethal spells";

            _print_final_god_abil_desc(which_god,
                                       "You can memorise " + offer + ".",
                                       ABIL_NON_ABILITY);
        }
    }
    else if (which_god == GOD_GOZAG)
    {
        have_any = true;
        _print_final_god_abil_desc(which_god,
                                   "You passively detect gold.",
                                   ABIL_NON_ABILITY);
        _print_final_god_abil_desc(which_god,
                                   uppercase_first(god_name(which_god))
                                   + " turns your defeated foes' bodies"
                                   + " to gold.",
                                   ABIL_NON_ABILITY);
        _print_final_god_abil_desc(which_god,
                                   "Your enemies may become distracted by "
                                   "glittering piles of gold.",
                                   ABIL_NON_ABILITY);
    }
    else if (which_god == GOD_QAZLAL)
    {
        have_any = true;
        _print_final_god_abil_desc(which_god,
                                   "You are immune to your own clouds.",
                                   ABIL_NON_ABILITY);
    }

    // mv: No abilities (except divine protection) under penance
    if (!player_under_penance())
    {
        vector<ability_type> abilities = get_god_abilities(true, true);
        for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
            if ((you_worship(GOD_GOZAG)
                 && you.gold >= get_gold_cost(abilities[i])
                 || !you_worship(GOD_GOZAG)
                 && you.piety >= piety_breakpoint(i))
                && _print_god_abil_desc(which_god, i))
            {
                have_any = true;
            }
    }

    string extra = get_linebreak_string(_religion_help(which_god),
                                        numcols).c_str();
    if (!extra.empty())
    {
        have_any = true;
        _print_final_god_abil_desc(which_god, extra, ABIL_NON_ABILITY);
    }

    if (!have_any)
        cprintf("None.\n");
}

void describe_god(god_type which_god, bool give_title)
{
    clrscr();

    if (give_title)
    {
        textcolor(WHITE);
        cprintf("                                  Religion\n");
        textcolor(LIGHTGREY);
    }

    if (which_god == GOD_NO_GOD) //mv: No god -> say it and go away.
    {
        cprintf("\nYou are not religious.");
        get_ch();
        return;
    }

    // Print long god's name.
    textcolor(god_colour(which_god));
    cprintf("%s", uppercase_first(god_name(which_god, true)).c_str());
    cprintf("\n\n");

    // Print god's description.
    textcolor(LIGHTGREY);

    string god_desc = getLongDescription(god_name(which_god));
    const int numcols = get_number_of_cols() - 1;
    cprintf("%s", get_linebreak_string(god_desc.c_str(), numcols).c_str());

    // Title only shown for our own god.
    if (you_worship(which_god))
    {
        // Print title based on piety.
        cprintf("\nTitle - ");
        textcolor(god_colour(which_god));

        string title = god_title(which_god, you.species, you.piety);
        cprintf("%s", title.c_str());
    }

    // mv: Now let's print favour as Brent suggested.
    // I know these messages aren't perfect so if you can think up
    // something better, do it.

    textcolor(LIGHTGREY);
    cprintf("\n\nFavour - ");
    textcolor(god_colour(which_god));

    //mv: Player is praying at altar without appropriate religion.
    // It means player isn't checking his own religion and so we only
    // display favour and go out.
    if (!you_worship(which_god))
    {
        textcolor(god_colour(which_god));
        cprintf(_god_penance_message(which_god).c_str());
    }
    else
    {
        cprintf(_describe_favour(which_god).c_str());
        if (which_god == GOD_ASHENZARI)
            cprintf("\n%s", ash_describe_bondage(ETF_ALL, true).c_str());

        _describe_god_powers(which_god, numcols);
    }

    if (_check_description_toggle())
        _detailed_god_description(which_god);
}

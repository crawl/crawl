/**
 * @file
 * @brief Functions and data structures dealing with the syntax,
 *        morphology, and orthography of the English language.
**/

#include "AppHdr.h"

#include "english.h"

#include <cstddef>
#include <cwctype>
#include <string>

#include "stringutil.h"

const char * const standard_plural_qualifiers[] =
{
    " of ", " labelled ", " from ", nullptr
};

bool is_vowel(const char32_t chr)
{
    const char low = towlower(chr);
    return low == 'a' || low == 'e' || low == 'i' || low == 'o' || low == 'u';
}

// Pluralises a monster or item name. This'll need to be updated for
// correctness whenever new monsters/items are added.
string pluralise(const string &name, const char * const qualifiers[],
                 const char * const no_qualifier[])
{
    string::size_type pos;

    if (qualifiers)
    {
        for (int i = 0; qualifiers[i]; ++i)
            if ((pos = name.find(qualifiers[i])) != string::npos
                && !ends_with(name, no_qualifier))
            {
                return pluralise(name.substr(0, pos)) + name.substr(pos);
            }
    }

    if (!name.empty() && name[name.length() - 1] == ')'
        && (pos = name.rfind(" (")) != string::npos)
    {
        return pluralise(name.substr(0, pos)) + name.substr(pos);
    }

    if (!name.empty() && name[name.length() - 1] == ']'
        && (pos = name.rfind(" [")) != string::npos)
    {
        return pluralise(name.substr(0, pos)) + name.substr(pos);
    }

    const string lowname = lowercase_string(name);

    if (ends_with(lowname, "us"))
    {
        if (ends_with(lowname, "lotus") || ends_with(lowname, "status"))
            return name + "es";
        // Fungus, ufetubus, for instance.
        return name.substr(0, name.length() - 2) + "i";
    }
    else if (ends_with(lowname, "larva") || ends_with(lowname, "antenna")
             || ends_with(lowname, "hypha") || ends_with(lowname, "noma"))
    {
        return name + "e";
    }
    else if (ends_with(lowname, "ex"))
    {
        // Vortex; vortexes is legal, but the classic plural is cooler.
        return name.substr(0, name.length() - 2) + "ices";
    }
    else if (ends_with(lowname, "mosquito") || ends_with(lowname, "ss"))
        return name + "es";
    else if (ends_with(lowname, "cyclops"))
        return name.substr(0, name.length() - 1) + "es";
    else if (lowname == "catoblepas")
        return "catoblepae";
    else if (lowname == "stratum")
        return "strata";
    else if (ends_with(lowname, "s"))
        return name;
    else if (ends_with(lowname, "y"))
    {
        if (lowname == "y")
            return name + "s"; // oh ys.
        // day -> days, boy -> boys, etc
        if (is_vowel(lowname[lowname.length() - 2]))
            return name + "s";
        // jelly -> jellies
        return name.substr(0, name.length() - 1) + "ies";
    }
    else if (ends_with(lowname, "fe"))
    {
        // knife -> knives
        return name.substr(0, name.length() - 2) + "ves";
    }
    else if (ends_with(lowname, "staff"))
    {
        // staff -> staves
        return name.substr(0, name.length() - 2) + "ves";
    }
    else if (ends_with(lowname, "f") && !ends_with(lowname, "ff"))
    {
        // elf -> elves, but not hippogriff -> hippogrives.
        // TODO: if someone defines a "goblin chief", this should be revisited.
        return name.substr(0, name.length() - 1) + "ves";
    }
    else if (ends_with(lowname, "mage") && !ends_with(lowname, "damage"))
    {
        // mage -> magi
        return name.substr(0, name.length() - 1) + "i";
    }
    else if (lowname == "gold"                 || lowname == "oni"
             || ends_with(lowname, "fish")     || ends_with(lowname, "folk")
             || ends_with(lowname, "spawn")    || ends_with(lowname, "tengu")
             || ends_with(lowname, "sheep")    || ends_with(lowname, "swine")
             || ends_with(lowname, "efreet")   || ends_with(lowname, "jiangshi")
             || ends_with(lowname, "raiju")    || ends_with(lowname, "meliai"))
    {
        return name;
    }
    else if (ends_with(lowname, "ch") || ends_with(lowname, "sh")
             || ends_with(lowname, "x"))
    {
        // To handle cockroaches, sphinxes, and bushes.
        return name + "es";
    }
    else if (ends_with(lowname, "simulacrum") || ends_with(lowname, "eidolon"))
    {
        // simulacrum -> simulacra (correct Latin pluralisation)
        // also eidolon -> eidola (correct Greek pluralisation)
        return name.substr(0, name.length() - 2) + "a";
    }
    else if (ends_with(lowname, "djinni"))
    {
        // djinni -> djinn.
        return name.substr(0, name.length() - 1);
    }
    else if (lowname == "foot")
        return "feet"; // TODO: handle Foot correctly
    else if (lowname == "ophan" || lowname == "cherub" || lowname == "seraph")
    {
        // Unlike "angel" which is fully assimilated, and "cherub" and "seraph"
        // which may be pluralised both ways, "ophan" always uses Hebrew
        // pluralisation.
        return name + "im";
    }
    else if (ends_with(lowname, "arachi"))
    {
        // Barachi -> Barachim. Kind of Hebrew? Kind of goofy.
        // (not sure if this is ever used...)
        return name + "m";
    }
    else if (lowname == "ushabti")
    {
        // ushabti -> ushabtiu (correct ancient Egyptian pluralisation)
        return name + "u";
    }
    else if (lowname == "Tzitzimitl")
    {
        // Tzitzimitl -> Tzitzimimeh (correct Nahuatl pluralisation)
        return name.substr(0, name.length() - 2) + "meh";
    }
    // "<name>'s ghost" -> "ghosts called <name>".
    pos = lowname.find("'s ghost");
    if (string::npos != pos)
        return string(name, 0, pos).insert(0, "ghosts called ");

    return name + "s";
}

// For monster names ending with these suffixes, we pluralise directly without
// attempting to use the "of" rule. For instance:
//
//      moth of wrath           => moths of wrath but
//      moth of wrath zombie    => moth of wrath zombies.
static const char * const _monster_suffixes[] =
{
    "zombie", "skeleton", "simulacrum", nullptr
};

string pluralise_monster(const string &name)
{
    return pluralise(name, standard_plural_qualifiers, _monster_suffixes);
}

string apostrophise(const string &name)
{
    if (name.empty())
        return name;

    if (name == "you" || name == "You")
        return name + "r";

    if (name == "it" || name == "It")
        return name + "s";

    if (name == "itself")
        return "its own";

    if (name == "himself")
        return "his own";

    if (name == "herself")
        return "her own";

    if (name == "themselves" || name == "themself")
        return "their own";

    if (name == "yourself")
        return "your own";

    // We're going with the assumption that we're finding the possessive of
    // singular nouns ending in 's' more often than that of plural nouns.
    // No matter what, we're going to get some cases wrong.

    // const char lastc = name[name.length() - 1];
    return name + /*(lastc == 's' ? "'" :*/ "'s" /*)*/;
}

/**
 * Get the singular form of a given plural-agreeing verb.
 *
 * An absurd simplification of the english language, but for our purposes...
 *
 * @param verb   A plural-agreeing or infinitive verb
 *               ("smoulder", "are", "be", etc.) or phrasal verb
 *               ("shout at", "make way for", etc.)
 * @param plural Should we conjugate the verb for the plural rather than
 *               the singular?
 * @return       The singular ("smoulders", "is", "shouts at") or plural-
 *               agreeing ("smoulder", "are", "shout at", etc.) finite form
 *               of the verb, depending on \c plural .
 */
string conjugate_verb(const string &verb, bool plural)
{
    if (!verb.empty() && verb[0] == '!')
        return verb.substr(1);

    // Conjugate the first word of a phrase (e.g. "release spores at")
    const size_t space = verb.find(" ");
    if (space != string::npos)
    {
        return conjugate_verb(verb.substr(0, space), plural)
               + verb.substr(space);
    }

    // Only one verb in English differs between infinitive and plural.
    if (plural)
        return verb == "be" ? "are" : verb;

    if (verb == "are" || verb == "be")
        return "is";

    if (verb == "have")
        return "has";

    if (ends_with(verb, "f") || ends_with(verb, "fe")
        || ends_with(verb, "y"))
    {
        return verb + "s";
    }

    return pluralise(verb);
}

static const char * const _pronoun_declension[][NUM_PRONOUN_CASES] =
{
    // subj  poss    refl        obj
    { "it",  "its",  "itself",   "it"  }, // neuter
    { "he",  "his",  "himself",  "him" }, // masculine
    { "she", "her",  "herself",  "her" }, // feminine
    { "you", "your", "yourself", "you" }, // 2nd person
    { "they", "their", "themself", "them" }, // neutral
};

const char *decline_pronoun(gender_type gender, pronoun_type variant)
{
    COMPILE_CHECK(ARRAYSZ(_pronoun_declension) == NUM_GENDERS);
    ASSERT_RANGE(gender, 0, NUM_GENDERS);
    ASSERT_RANGE(variant, 0, NUM_PRONOUN_CASES);
    return _pronoun_declension[gender][variant];
}

// Takes a lowercase verb stem like "walk", "glid" or "wriggl"
// (as could be used for "walking", "gliding", or "wriggler")
// and turn it into the present tense form.
// TODO: make this more general. (Does english have rules?)
string walk_verb_to_present(string verb)
{
    if (verb == "wriggl")
        return "wriggle";
    if (verb == "glid")
    {
        return "walk"; // it's a lie! tengu only get this
                       // verb when they can't fly!
    }
    return verb;
}

static string _tens_in_words(unsigned num)
{
    static const char *numbers[] =
    {
        "", "one", "two", "three", "four", "five", "six", "seven",
        "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
        "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
    };
    static const char *tens[] =
    {
        "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy",
        "eighty", "ninety"
    };

    if (num < 20)
        return numbers[num];

    int ten = num / 10, digit = num % 10;
    return string(tens[ten]) + (digit ? string("-") + numbers[digit] : "");
}

static string _join_strings(const string &a, const string &b)
{
    if (!a.empty() && !b.empty())
        return a + " " + b;

    return a.empty() ? b : a;
}

static string _hundreds_in_words(unsigned num)
{
    unsigned dreds = num / 100, tens = num % 100, ones = num % 10;
    string sdreds = dreds? _tens_in_words(dreds) +
    ((tens || ones)? " hundred and" : " hundred") : "";
    string stens  = tens? _tens_in_words(tens) : "";
    return _join_strings(sdreds, stens);
}

static string _number_in_words(unsigned num, unsigned period)
{
    static const char * const periods[] = {
        "", " thousand", " million", " billion", " trillion"
    };

    ASSERT(period < ARRAYSZ(periods));

    // Handle "eighteen million trillion", should unsigned go that high.
    if (period == ARRAYSZ(periods) - 1)
        return _number_in_words(num, 0) + periods[period];

    unsigned thousands = num % 1000, rest = num / 1000;
    if (!rest && !thousands)
        return "zero";

    return _join_strings((rest? _number_in_words(rest, period + 1) : ""),
                        (thousands? _hundreds_in_words(thousands)
                                    + periods[period]
                                  : ""));
}

string number_in_words(unsigned num)
{
    return _number_in_words(num, 0);
}

static string _number_to_string(unsigned number, bool in_words)
{
    return in_words ? number_in_words(number) : to_string(number);
}

// Naively prefix A/an to a noun.
string article_a(const string &name, bool lowercase)
{
    if (!name.length())
        return name;

    const char *a  = lowercase? "a "  : "A ";
    const char *an = lowercase? "an " : "An ";
    switch (name[0])
    {
        case 'a': case 'e': case 'i': case 'o': case 'u':
        case 'A': case 'E': case 'I': case 'O': case 'U':
            // XXX: Hack for hydras.
            if (starts_with(name, "one-"))
                return a + name;
            return an + name;
        case '1':
            // XXX: Hack^2 for hydras.
            if (starts_with(name, "11-") || starts_with(name, "18-"))
                return an + name;
            return a + name;
        case '8':
            // Eighty, eight hundred, eight thousand, ...
            return an + name;
        default:
            return a + name;
    }
}

string apply_description(description_level_type desc, const string &name,
                         int quantity, bool in_words)
{
    switch (desc)
    {
    case DESC_THE:
        return "the " + name;
    case DESC_A:
        return quantity > 1 ? _number_to_string(quantity, in_words) + name
                            : article_a(name, true);
    case DESC_YOUR:
        return "your " + name;
    case DESC_PLAIN:
    default:
        return name;
    }
}

string thing_do_grammar(description_level_type dtype, string desc,
                        bool ignore_case)
{
    // Avoid double articles.
    if (starts_with(desc, "the ") || starts_with(desc, "The ")
        || starts_with(desc, "a ") || starts_with(desc, "A ")
        || starts_with(desc, "an ") || starts_with(desc, "An ")
        || starts_with(desc, "some ") || starts_with(desc, "Some "))
    {
        if (dtype == DESC_THE || dtype == DESC_A)
            dtype = DESC_PLAIN;
    }

    if (dtype == DESC_PLAIN || !ignore_case && isupper(desc[0]))
        return desc;

    switch (dtype)
    {
    case DESC_THE:
        return "the " + desc;
    case DESC_A:
        return article_a(desc, true);
    case DESC_NONE:
        return "";
    default:
        return desc;
    }
}

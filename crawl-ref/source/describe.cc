/*
 *  File:       describe.cc
 *  Summary:    Functions used to print information about various game objects.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "describe.h"
#include "database.h"

#include <stdio.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <numeric>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "cio.h"
#include "debug.h"
#include "decks.h"
#include "fight.h"
#include "food.h"
#include "ghost.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "macro.h"
#include "mapmark.h"
#include "menu.h"
#include "message.h"
#include "monstuff.h"
#include "mon-util.h"
#include "newgame.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills2.h"
#include "spells3.h"
#include "spl-book.h"
#include "stuff.h"
#include "spl-util.h"
#include "transfor.h"
#include "tutorial.h"
#include "xom.h"

#define LONG_DESC_KEY "long_desc_key"

// ========================================================================
//      Internal Functions
// ========================================================================

//---------------------------------------------------------------
//
// append_value
//
// Appends a value to the string. If plussed == 1, will add a + to
// positive values (itoa always adds - to -ve ones).
//
//---------------------------------------------------------------
static void _append_value( std::string & description, int valu, bool plussed )
{
    if (valu >= 0 && plussed == 1)
        description += "+";

    char value_str[80];

    itoa( valu, value_str, 10 );

    description += value_str;
}

static int _count_desc_lines(const std::string _desc, const int width)
{
   std::string desc = get_linebreak_string(_desc, width);

   int count = 0;
   for (int i = 0, size = desc.size(); i < size; i++)
   {
       const char ch = desc[i];

       if (ch == '\n' || ch == '$')
           count++;
   }

  return count;
}

//---------------------------------------------------------------
//
// print_description
//
// Takes a descrip string filled up with stuff from other functions,
// and displays it with minor formatting to avoid cut-offs in mid
// word and such. The character $ is interpreted as a CR.
//
//---------------------------------------------------------------
void print_description(const std::string &d, const std::string title,
                       const std::string suffix, const std::string prefix,
                       const std::string footer, const std::string quote)
{
    const unsigned int lineWidth = get_number_of_cols()  - 1;
    const          int height    = get_number_of_lines();

    std::string desc;

    if (title.empty())
       desc = d;
    else
    {
        desc = title + "$$";
        desc += d;
    }

    int num_lines = _count_desc_lines(desc, lineWidth) + 1;

    const int suffix_lines = _count_desc_lines(suffix, lineWidth);
    const int prefix_lines = _count_desc_lines(prefix, lineWidth);
    const int footer_lines = _count_desc_lines(footer, lineWidth)
                             + (footer.empty() ? 0 : 1);
    const int quote_lines  = _count_desc_lines(quote, lineWidth);

    // Prefer the footer over the suffix.
    if (num_lines + suffix_lines + footer_lines <= height)
    {
        desc = desc + suffix;
        num_lines += suffix_lines;
    }

    // Prefer the footer over the prefix.
    if (num_lines + prefix_lines + footer_lines <= height)
    {
        desc = prefix + desc;
        num_lines += prefix_lines;
    }

    // Prefer the footer over the quote.
    if (num_lines + footer_lines + quote_lines + 1 <= height)
    {
        if (!desc.empty())
        {
            desc += "$";
            num_lines++;
        }
        desc = desc + quote;
        num_lines += quote_lines;
    }

    if (!footer.empty() && num_lines + footer_lines <= height)
    {
        const int bottom_line = std::min(std::max(24, num_lines + 2),
                                         height - footer_lines + 1);
        const int newlines = bottom_line - num_lines;

        if (newlines >= 0)
        {
            desc.append(newlines, '\n');
            desc = desc + footer;
        }
    }

    std::string::size_type nextLine = std::string::npos;
    unsigned int  currentPos = 0;

    clrscr();
    textcolor(LIGHTGREY);

    while (currentPos < desc.length())
    {
        if (currentPos != 0)
            cgotoxy(1, wherey() + 1);

        // See if $ sign is within one lineWidth.
        nextLine = desc.find('$', currentPos);

        if (nextLine >= currentPos && nextLine < currentPos + lineWidth)
        {
            cprintf("%s",
                    (desc.substr(currentPos, nextLine-currentPos)).c_str());
            currentPos = nextLine + 1;
            continue;
        }

        // Handle real line breaks.  No substitutions necessary, just update
        // the counts.
        nextLine = desc.find('\n', currentPos);
        if (nextLine >= currentPos && nextLine < currentPos + lineWidth)
        {
            cprintf("%s",
                    (desc.substr(currentPos, nextLine-currentPos)).c_str());
            currentPos = nextLine + 1;
            continue;
        }

        // No newline -- see if rest of string will fit.
        if (currentPos + lineWidth >= desc.length())
        {
            cprintf((desc.substr(currentPos)).c_str());
            return;
        }


        // Ok, try to truncate at space.
        nextLine = desc.rfind(' ', currentPos + lineWidth);

        if (nextLine > 0)
        {
            cprintf((desc.substr(currentPos, nextLine - currentPos)).c_str());
            currentPos = nextLine + 1;
            continue;
        }

        // Oops.  Just truncate.
        nextLine = currentPos + lineWidth;

        nextLine = std::min(d.length(), nextLine);

        cprintf((desc.substr(currentPos, nextLine - currentPos)).c_str());
        currentPos = nextLine;
    }
}

const char* jewellery_base_ability_string(int subtype)
{
    switch(subtype)
    {
    case RING_REGENERATION:      return "Regen";
    case RING_SUSTAIN_ABILITIES: return "SustAb";
    case RING_SUSTENANCE:        return "Hunger-";
    case RING_WIZARDRY:          return "Wiz";
    case RING_FIRE:              return "Fire";
    case RING_ICE:               return "Ice";
    case RING_TELEPORT_CONTROL:  return "cTele";
    case AMU_RESIST_SLOW:        return "rSlow";
    case AMU_CLARITY:            return "Clar";
    case AMU_WARDING:            return "Ward";
    case AMU_RESIST_CORROSION:   return "rCorr";
    case AMU_THE_GOURMAND:       return "Gourm";
    case AMU_CONSERVATION:       return "Cons";
    case AMU_CONTROLLED_FLIGHT:  return "cFly";
    case AMU_RESIST_MUTATION:    return "rMut";
    }
    return "";
}


#define known_proprt(prop) (proprt[(prop)] && known[(prop)])

struct property_annotators
{
    const char* name;
    randart_prop_type prop;
    int spell_out;              // 0: "+3", 1: "+++", 2: value doesn't matter
};

static std::vector<std::string> _randart_propnames( const item_def& item )
{
    randart_properties_t  proprt;
    randart_known_props_t known;
    randart_desc_properties( item, proprt, known, true );

    std::vector<std::string> propnames;

    // list the following in rough order of importance
    const property_annotators propanns[] = {

        // (Generally) negative attributes
        // These come first, so they don't get chopped off!
        { "-CAST",  RAP_PREVENT_SPELLCASTING,  2 },
        { "-TELE",  RAP_PREVENT_TELEPORTATION, 2 },
        { "MUT",    RAP_MUTAGENIC,             2 }, // handled specially
        { "*Rage",  RAP_ANGRY,                 2 },
        { "*TELE",  RAP_CAUSE_TELEPORTATION,   2 },
        { "Hunger", RAP_METABOLISM,            2 }, // handled specially
        { "Noisy",  RAP_NOISES,                2 },

        // Evokable abilities come second
        { "+Blink", RAP_BLINK,                 2 },
        { "+Tele",  RAP_CAN_TELEPORT,          2 },
        { "+Rage",  RAP_BERSERK,               2 },
        { "+Inv",   RAP_INVISIBLE,             2 },
        { "+Lev",   RAP_LEVITATE,              2 },
        { "+Map",   RAP_MAPPING,               2 },

        // Resists, also really important
        { "rElec",  RAP_ELECTRICITY,           2 },
        { "rPois",  RAP_POISON,                2 },
        { "rF",     RAP_FIRE,                  1 },
        { "rC",     RAP_COLD,                  1 },
        { "rN",     RAP_NEGATIVE_ENERGY,       1 },
        { "MR",     RAP_MAGIC,                 2 },

        // Quantitative attributes
        { "AC",     RAP_AC,                    0 },
        { "EV",     RAP_EVASION,               0 },
        { "Str",    RAP_STRENGTH,              0 },
        { "Dex",    RAP_DEXTERITY,             0 },
        { "Int",    RAP_INTELLIGENCE,          0 },
        { "Acc",    RAP_ACCURACY,              0 },
        { "Dam",    RAP_DAMAGE,                0 },

        // Qualitative attributes
        { "MP",     RAP_MAGICAL_POWER,         0 },
        { "SInv",   RAP_EYESIGHT,              2 },
        { "Stlth",  RAP_STEALTH,               2 }, // handled specially
        { "Curse",  RAP_CURSED,                2 },
    };

    // For randart jewellery, note the base jewellery type if it's not
    // covered by randart_desc_properties()
    if (item.base_type == OBJ_JEWELLERY
        && item_ident(item, ISFLAG_KNOW_PROPERTIES))
    {
        const std::string type = jewellery_base_ability_string(item.sub_type);
        if (!type.empty())
            propnames.push_back(type);
    }
    else if (item.base_type == OBJ_WEAPONS
             && item_ident(item, ISFLAG_KNOW_TYPE))
    {
        std::string ego = weapon_brand_name(item, true);
        if (!ego.empty())
        {
            // XXX: Ugly hack to remove the brackets...
            ego = ego.substr(2, ego.length() - 3);

            // ... and another one for adding a comma if needed.
            for (unsigned i = 0; i < ARRAYSZ(propanns); ++i)
                if (known_proprt(propanns[i].prop)
                    && propanns[i].prop != RAP_BRAND)
                {
                    ego += ",";
                    break;
                }

            propnames.push_back(ego);
        }
    }

    for (unsigned i = 0; i < ARRAYSZ(propanns); ++i)
    {
        if (known_proprt(propanns[i].prop))
        {
            const int val = proprt[propanns[i].prop];

            // Don't show rF+/rC- for =Fire, or vice versa for =Ice.
            if (item.base_type == OBJ_JEWELLERY)
            {
                if (item.sub_type == RING_FIRE
                    && (propanns[i].prop == RAP_FIRE && val == 1
                        || propanns[i].prop == RAP_COLD && val == -1))
                {
                    continue;
                }
                if (item.sub_type == RING_ICE
                    && (propanns[i].prop == RAP_COLD && val == 1
                        || propanns[i].prop == RAP_FIRE && val == -1))
                {
                    continue;
                }
            }

            std::ostringstream work;
            switch (propanns[i].spell_out)
            {
            case 0: // e.g. AC+4
                work << std::showpos << propanns[i].name << val;
                break;
            case 1: // e.g. F++
            {
                int sval = std::min(std::abs(val), 3);
                work << propanns[i].name
                     << std::string(sval, (val > 0 ? '+' : '-'));
                break;
            }
            case 2: // e.g. rPois or SInv
                if (propanns[i].prop == RAP_CURSED && val < 1)
                    continue;

                work << propanns[i].name;

                // these need special handling, so we don't give anything away
                if (propanns[i].prop == RAP_METABOLISM && val > 2
                    || propanns[i].prop == RAP_MUTAGENIC && val > 3
                    || propanns[i].prop == RAP_STEALTH && val > 20)
                {
                    work << "+";
                }
                else if (propanns[i].prop == RAP_STEALTH && val < 0)
                {
                    if (val < -20)
                        work << "--";
                    else
                        work << "-";
                }
                break;
            }
            propnames.push_back(work.str());
        }
    }

    return propnames;
}

// Remove randart auto-inscription.  Do it once for each property
// string, rather than the return value of randart_auto_inscription(),
// in case more information about the randart has been learned since
// the last auto-inscription.
static void _trim_randart_inscrip( item_def& item )
{
    std::vector<std::string> propnames = _randart_propnames(item);

    for (unsigned int i = 0; i < propnames.size(); i++)
    {
        item.inscription = replace_all(item.inscription, propnames[i]+",", "");
        item.inscription = replace_all(item.inscription, propnames[i],     "");
    }
    trim_string(item.inscription);
}

std::string randart_auto_inscription( const item_def& item )
{
    if (item.base_type == OBJ_BOOKS)
        return("");

    const std::vector<std::string> propnames = _randart_propnames(item);

    return comma_separated_line(propnames.begin(), propnames.end(),
                                " ", " ");
}

void add_autoinscription( item_def &item, std::string ainscrip)
{
    // Remove previous randart inscription.
    _trim_randart_inscrip(item);

    if (!item.inscription.empty())
    {
        if (ends_with(item.inscription, ","))
            item.inscription += " ";
        else
            item.inscription += ", ";
    }

    item.inscription += ainscrip;
}

struct property_descriptor
{
    randart_prop_type property;
    const char* desc;           // If it contains %d, will be replaced by value.
    bool is_graded_resist;
};

static std::string _randart_descrip( const item_def &item )
{
    std::string description;

    randart_properties_t  proprt;
    randart_known_props_t known;
    randart_desc_properties( item, proprt, known );

    const property_descriptor propdescs[] = {
        { RAP_AC, "It affects your AC (%d).", false },
        { RAP_EVASION, "It affects your evasion (%d).", false},
        { RAP_STRENGTH, "It affects your strength (%d).", false},
        { RAP_INTELLIGENCE, "It affects your intelligence (%d).", false},
        { RAP_DEXTERITY, "It affects your dexterity (%d).", false},
        { RAP_ACCURACY, "It affects your accuracy (%d).", false},
        { RAP_DAMAGE, "It affects your damage-dealing abilities (%d).", false},
        { RAP_FIRE, "fire", true},
        { RAP_COLD, "cold", true},
        { RAP_ELECTRICITY, "It insulates you from electricity.", false},
        { RAP_POISON, "It protects you from poison.", false},
        { RAP_NEGATIVE_ENERGY, "negative energy", true},
        { RAP_MAGIC, "It increases your resistance to enchantments.", false},
        { RAP_MAGICAL_POWER, "It affects your mana capacity (%d).", false},
        { RAP_EYESIGHT, "It enhances your eyesight.", false},
        { RAP_INVISIBLE, "It lets you turn invisible.", false},
        { RAP_LEVITATE, "It lets you levitate.", false},
        { RAP_BLINK, "It lets you blink.", false},
        { RAP_CAN_TELEPORT, "It lets you teleport.", false},
        { RAP_BERSERK, "It lets you go berserk.", false},
        { RAP_MAPPING, "It lets you sense your surroundings.", false},
        { RAP_NOISES, "It makes noises.", false},
        { RAP_PREVENT_SPELLCASTING, "It prevents spellcasting.", false},
        { RAP_CAUSE_TELEPORTATION, "It causes teleportation.", false},
        { RAP_PREVENT_TELEPORTATION, "It prevents most forms of teleportation.",
          false},
        { RAP_ANGRY,  "It makes you angry.", false},
        { RAP_CURSED, "It may recurse itself.", false}
    };

    for (unsigned i = 0; i < ARRAYSZ(propdescs); ++i)
    {
        if (known_proprt(propdescs[i].property))
        {
            // Only randarts with RAP_CURSED > 0 may recurse themselves.
            if (propdescs[i].property == RAP_CURSED
                && proprt[propdescs[i].property] < 1)
            {
                continue;
            }

            std::string sdesc = propdescs[i].desc;

            // FIXME Not the nicest hack.
            char buf[80];
            snprintf(buf, sizeof buf, "%+d", proprt[propdescs[i].property]);
            sdesc = replace_all(sdesc, "%d", buf);

            if (propdescs[i].is_graded_resist)
            {
                int idx = proprt[propdescs[i].property] + 3;
                idx = std::min(idx, 6);
                idx = std::max(idx, 0);

                const char* prefixes[] = {
                    "It makes you extremely vulnerable to ",
                    "It makes you very vulnerable to ",
                    "It makes you vulnerable to ",
                    "Buggy descriptor!",
                    "It protects you from ",
                    "It greatly protects you from ",
                    "It renders you almost immune to "
                };
                sdesc = prefixes[idx] + sdesc + '.';
            }

            description += '$';
            description += sdesc;
        }
    }

    // Some special cases which don't fit into the above.
    if (known_proprt( RAP_METABOLISM ))
    {
        if (proprt[ RAP_METABOLISM ] >= 3)
            description += "$It greatly speeds your metabolism.";
        else if (proprt[ RAP_METABOLISM ])
            description += "$It speeds your metabolism. ";
    }

    if (known_proprt( RAP_STEALTH ))
    {
        const int stval = proprt[RAP_STEALTH];
        char buf[80];
        snprintf(buf, sizeof buf, "$It makes you %s%s stealthy.",
                 (stval < -20 || stval > 20) ? "much " : "",
                 (stval < 0) ? "less" : "more");
        description += buf;
    }

    if (known_proprt( RAP_MUTAGENIC ))
    {
        if (proprt[ RAP_MUTAGENIC ] > 3)
            description += "$It glows with mutagenic radiation.";
        else
            description += "$It emits mutagenic radiation.";
    }

    if (is_unrandom_artefact( item ))
    {
        const char *desc = unrandart_descrip( 0, item );
        if (desc[0] != 0)
        {
            description += "$$";
            description += desc;
        }
    }

    return description;
}
#undef known_proprt

static const char *trap_names[] =
{
    "dart", "arrow", "spear", "axe",
    "teleport", "alarm", "blade",
    "bolt", "net", "zot", "needle",
    "shaft"
};

const char *trap_name(trap_type trap)
{
    ASSERT(NUM_TRAPS == sizeof(trap_names) / sizeof(*trap_names));

    if (trap >= TRAP_DART && trap < NUM_TRAPS)
        return trap_names[ static_cast<int>( trap ) ];
    return (NULL);
}

int str_to_trap(const std::string &s)
{
    ASSERT(NUM_TRAPS == sizeof(trap_names) / sizeof(*trap_names));

    // allow a couple of synonyms
    if (s == "random" || s == "any")
        return (TRAP_RANDOM);
    else if (s == "suitable")
        return (TRAP_INDEPTH);
    else if (s == "nonteleport" || s == "noteleport"
             || s == "nontele" || s == "notele")
    {
        return (TRAP_NONTELEPORT);
    }

    for (int i = 0; i < NUM_TRAPS; ++i)
        if (trap_names[i] == s)
            return (i);

    return (-1);
}

//---------------------------------------------------------------
//
// describe_demon
//
// Describes the random demons you find in Pandemonium.
//
//---------------------------------------------------------------
static std::string _describe_demon(const monsters &mons)
{
    ASSERT(mons.ghost.get());

    const ghost_demon &ghost = *mons.ghost;

    const long seed =
        std::accumulate(ghost.name.begin(), ghost.name.end(), 0L) *
        ghost.name.length();

    rng_save_excursion exc;
    seed_rng( seed );

    const char* body_descs[] = {
        " huge, barrel-shaped ",
        " wispy, insubstantial ",
        " spindly ",
        " skeletal ",
        " horribly deformed ",
        " spiny ",
        " waif-like ",
        " scaly ",
        " sickeningly deformed ",
        " bruised and bleeding ",
        " sickly ",
        " mass of writhing tentacles for a ",
        " mass of ropey tendrils for a ",
        " tree trunk-like ",
        " hairy ",
        " furry ",
        " fuzzy ",
        "n obese ",
        " fat ",
        " slimy ",
        " wrinkled ",
        " metallic ",
        " glassy ",
        " crystalline ",
        " muscular ",
        "n icky ",
        " swollen ",
        " lumpy ",
        "n armoured ",
        " carapaced ",
        " slender "
    };

    const char* wing_names[] = {
        " with small insectoid wings",
        " with large insectoid wings",
        " with moth-like wings",
        " with butterfly wings",
        " with huge, bat-like wings",
        " with fleshy wings",
        " with small, bat-like wings",
        " with hairy wings",
        " with great feathered wings",
        " with shiny metal wings"
    };

    const char* lev_names[] = {
        " which hovers in mid-air",
        " with sacs of gas hanging from its back"
    };

    const char* nonfly_names[] = {
        " covered in tiny crawling spiders",
        " covered in tiny crawling insects",
        " and the head of a crocodile",
        " and the head of a hippopotamus",
        " and a cruel curved beak for a mouth",
        " and a straight sharp beak for a mouth",
        " and no head at all",
        " and a hideous tangle of tentacles for a mouth",
        " and an elephantine trunk",
        " and an evil-looking proboscis",
        " and dozens of eyes",
        " and two ugly heads",
        " and a long serpentine tail",
        " and a pair of huge tusks growing from its jaw",
        " and a single huge eye, in the centre of its forehead",
        " and spikes of black metal for teeth",
        " and a disc-shaped sucker for a head",
        " and huge, flapping ears",
        " and a huge, toothy maw in the centre of its chest",
        " and a giant snail shell on its back",
        " and a dozen heads",
        " and the head of a jackal",
        " and the head of a baboon",
        " and a huge, slobbery tongue",
        " which is covered in oozing lacerations",
        " and the head of a frog",
        " and the head of a yak",
        " and eyes out on stalks",
    };

    const char* misc_descs[] = {
        " It seethes with hatred of the living.",
        " Tiny orange flames dance around it.",
        " Tiny purple flames dance around it.",
        " It is surrounded by a weird haze.",
        " It glows with a malevolent light.",
        " It looks incredibly angry.",
        " It oozes with slime.",
        " It dribbles constantly.",
        " Mould grows all over it.",
        " It looks diseased.",
        " It looks as frightened of you as you are of it.",
        " It moves in a series of hideous convulsions.",
        " It moves with an unearthly grace.",
        " It hungers for your soul!",
        " It leaves a glistening oily trail.",
        " It shimmers before your eyes.",
        " It is surrounded by a brilliant glow.",
        " It radiates an aura of extreme power.",
    };

    std::ostringstream description;
    description << "A powerful demon, " << ghost.name << " has a"
                << RANDOM_ELEMENT(body_descs) << "body";

    switch (ghost.fly)
    {
    case FL_FLY:
        description << RANDOM_ELEMENT(wing_names);
        break;

    case FL_LEVITATE:
        description << RANDOM_ELEMENT(lev_names);
        break;

    case FL_NONE:  // does not fly
        if (!one_chance_in(4))
            description << RANDOM_ELEMENT(nonfly_names);
        break;
    }

    description << ".";

    if (x_chance_in_y(3, 40))
    {
        if (player_can_smell())
        {
            switch (random2(3))
            {
            case 0:
                description << " It stinks of brimstone.";
                break;
            case 1:
                description << " It is surrounded by a sickening stench.";
                break;
            case 2:
                description << " It smells like rotting flesh"
                            << (player_mutation_level(MUT_SAPROVOROUS) == 3 ?
                                " - yum!" : ".");
                break;
            }
        }
    }
    else if (coinflip())
        description << RANDOM_ELEMENT(misc_descs);

    return description.str();
}

void append_weapon_stats(std::string &description, const item_def &item)
{
    description += "$Damage rating: ";
    _append_value(description, property( item, PWPN_DAMAGE ), false);
    description += "   ";

    description += "Accuracy rating: ";
    _append_value(description, property( item, PWPN_HIT ), true);
    description += "    ";

    description += "Base attack delay: ";
    _append_value(description, property( item, PWPN_SPEED ) * 10, false);
   description += "%";
}

//---------------------------------------------------------------
//
// describe_weapon
//
//---------------------------------------------------------------
static std::string _describe_weapon(const item_def &item, bool verbose)
{
    std::string description;

    description.reserve(200);

    description = "";

    if (is_fixed_artefact( item ))
    {
        if (item_ident( item, ISFLAG_KNOW_PROPERTIES ) && item.special)
        {
            description += "$$";
            switch (item.special)
            {
            case SPWPN_SINGING_SWORD:
                description += "This blessed weapon loves nothing more "
                    "than to sing to its owner, "
                    "whether they want it to or not.";
                break;
            case SPWPN_WRATH_OF_TROG:
                description += "This was the favourite weapon of "
                    "the old god Trog, before it was lost one day. "
                    "It induces a bloodthirsty berserker rage in "
                    "anyone who uses it to strike another.";
                break;
            case SPWPN_SCYTHE_OF_CURSES:
                description += "This weapon carries a "
                    "terrible and highly irritating curse.";
                break;
            case SPWPN_MACE_OF_VARIABILITY:
                description += "It is rather unreliable.";
                break;
            case SPWPN_GLAIVE_OF_PRUNE:
                description += "It is the creation of a mad god, and "
                    "carries a curse which transforms anyone "
                    "possessing it into a prune. Fortunately, "
                    "the curse works very slowly, and one can "
                    "use it briefly with no consequences "
                    "worse than slightly purple skin and a few wrinkles.";
                break;
            case SPWPN_SCEPTRE_OF_TORMENT:
                description += "This truly accursed weapon is "
                    "an instrument of Hell.";
                break;
            case SPWPN_SWORD_OF_ZONGULDROK:
                description += "This dreadful weapon is used "
                    "at the user's peril.";
                break;
            case SPWPN_SWORD_OF_CEREBOV:
                description += "Eerie flames cover its twisted blade.";
                break;
            case SPWPN_STAFF_OF_DISPATER:
                description += "This legendary item can unleash "
                    "the fury of Hell.";
                break;
            case SPWPN_SCEPTRE_OF_ASMODEUS:
                description += "It carries some of the powers of "
                    "the arch-fiend Asmodeus.";
                break;
            case SPWPN_SWORD_OF_POWER:
                description += "It rewards the powerful with power "
                    "and the meek with weakness.";
                break;
            case SPWPN_STAFF_OF_OLGREB:
                description += "It was the magical weapon wielded by the "
                    "mighty wizard Olgreb before he met his "
                    "fate somewhere within these dungeons. It "
                    "grants its wielder resistance to the "
                    "effects of poison and increases their "
                    "ability to use venomous magic, and "
                    "carries magical powers which can be evoked.";
                break;
            case SPWPN_VAMPIRES_TOOTH:
                description += "It is lethally vampiric.";
                break;
            case SPWPN_STAFF_OF_WUCAD_MU:
                description += "Its power varies in proportion to "
                    "its wielder's intelligence. "
                    "Using it can be a bit risky.";
                break;
            }
            description += "$";
        }
        else if (item_type_known(item))
        {
            // We know it's an artefact type weapon, but not what it does.
            description += "$This weapon may have some hidden properties.";
        }
    }

    if (verbose)
        append_weapon_stats(description, item);

    if (!is_fixed_artefact( item ))
    {
        int spec_ench = get_weapon_brand( item );

        if (!is_random_artefact( item ) && !verbose)
            spec_ench = SPWPN_NORMAL;

        // special weapon descrip
        if (spec_ench != SPWPN_NORMAL && item_type_known(item))
        {
            description += "$$";

            switch (spec_ench)
            {
            case SPWPN_FLAMING:
                description += "It emits flame when wielded, "
                    "causing extra injury to most foes "
                    "and up to double damage against "
                    "particularly susceptible opponents.";
                break;
            case SPWPN_FREEZING:
                description += "It has been specially enchanted to "
                    "freeze those struck by it, causing "
                    "extra injury to most foes and "
                    "up to double damage against "
                    "particularly susceptible opponents.";
                break;
            case SPWPN_HOLY_WRATH:
                description += "It has been blessed by the Shining One "
                    "to cause great damage to the undead and the unholy "
                    "creatures of Hell or Pandemonium.";
                break;
            case SPWPN_ELECTROCUTION:
                description += "Occasionally upon striking a foe "
                    "it will discharge some electrical energy "
                    "and cause terrible harm.";
                break;
            case SPWPN_ORC_SLAYING:
                description += "It is especially effective against "
                    "all of orcish descent.";
                break;
            case SPWPN_DRAGON_SLAYING:
                description += "This legendary weapon is deadly to all "
                    "dragonkind. It also provides some protection from the "
                    "breath attacks of dragons and other creatures.";
                break;
            case SPWPN_VENOM:
                if (is_range_weapon(item))
                    description += "It poisons the unbranded ammo it fires.";
                else
                    description += "It poisons the flesh of those it strikes.";
                break;
            case SPWPN_PROTECTION:
                description += "It protects the one who wields it against "
                    "injury (+5 to AC).";
                break;
            case SPWPN_DRAINING:
                description += "A truly terrible weapon, "
                    "it drains the life of those it strikes.";
                break;
            case SPWPN_SPEED:
                description += "Attacks with this weapon take half as long "
                    "as usual.";
                break;
            case SPWPN_VORPAL:
                if (is_range_weapon(item))
                {
                    description += "Any ";
                    description += ammo_name( item );
                    description += " fired from it inflicts extra damage.";
                }
                else
                {
                    description += "It inflicts extra damage upon "
                        "your enemies.";
                }
                break;
            case SPWPN_FLAME:
                description += "It turns projectiles fired from it into "
                    "bolts of flame.";
                break;
            case SPWPN_FROST:
                description += "It turns projectiles fired from it into "
                    "bolts of frost.";
                break;
            case SPWPN_CHAOS:
                if (is_range_weapon(item))
                    description += "Each time it fires it turns the launched "
                        "projectile into a different, random type of bolt.";
                else
                    description += "Each time it hits an enemy it has a "
                        "different, random effect.";
                break;
            case SPWPN_VAMPIRICISM:
                description += "It inflicts no extra harm, "
                    "but heals its wielder somewhat when "
                    "it strikes a living foe.";
                break;
            case SPWPN_PAIN:
                description += "In the hands of one skilled in "
                    "necromantic magic it inflicts "
                    "extra damage on living creatures.";
                break;
            case SPWPN_DISTORTION:
                description += "It warps and distorts space around it. "
                    "Unwielding it can cause banishment or high damage.";
                break;
            case SPWPN_REACHING:
                description += "It can be evoked to extend its reach.";
                break;
            case SPWPN_RETURNING:
                description += "A skilled user can throw it in such a way "
                    "that it will return to its owner.";
                break;
            case SPWPN_PENETRATION:
                description += "Ammo fired by it will pass through the "
                    "targets it hits, potentially hitting all targets in "
                    "its path until it reaches maximum range.";
                break;
            case SPWPN_SHADOW:
                description += "If ammo fired by it kills a monster, "
                    "causing it to leave a corpse, the corpse will be "
                    "animated as a zombie friendly to the one who fired it.";
                break;
            }
        }

        if (is_random_artefact( item ))
        {
            std::string rand_desc = _randart_descrip( item );
            if (!rand_desc.empty())
            {
                description += "$$";
                description += rand_desc;
            }

            // Can't happen, right? (XXX)
            if (!item_ident(item, ISFLAG_KNOW_PROPERTIES)
                && item_type_known(item))
            {
                description += "$This weapon may have some hidden properties.";
            }
        }
    }

    const bool launcher = is_range_weapon(item);
    if (verbose)
    {
        description += "$$This weapon falls into the";

        const skill_type skill =
            is_range_weapon(item)? range_skill(item) : weapon_skill(item);

        description +=
            make_stringf(" '%s' category. ",
                         skill == SK_FIGHTING? "buggy" : skill_name(skill));

        if (!launcher)
        {
            switch (hands_reqd(item, player_size()))
            {
            case HANDS_ONE:
                description += "It is a one handed weapon";
                 break;
            case HANDS_HALF:
                description += "It can be used with one hand, or more "
                       "effectively with two (i.e. when not using a shield)";
                break;
            case HANDS_TWO:
                description += "It is a two handed weapon";
                break;
            case HANDS_DOUBLE:
                description += "It is a buggy weapon";
                break;
            }

            const int str_weight = weapon_str_weight(item.base_type, item.sub_type);

            if (str_weight >= 8)
                description += ", and it is best used by the strong";
            else if (str_weight > 5)
                description += ", and it is better for the strong";
            else if (str_weight <= 2)
                description += ", and it is best used by the dexterous";
            else if (str_weight < 5)
                description += ", and it is better for the dexterous";
            description += ".";
        }

    }

    if (verbose)
    {
        if (is_demonic(item) && !launcher)
            description += "$Demonspawn are more deadly with it.";
        else if (get_equip_race(item) != ISFLAG_NO_RACE)
        {
            unsigned long race = get_equip_race(item);

            if (race == ISFLAG_DWARVEN)
                description += "$It is well-crafted and very durable.";

            description += "$";
            description += (race == ISFLAG_DWARVEN) ? "Dwarves" :
                           (race == ISFLAG_ELVEN)   ? "Elves"
                                                    : "Orcs";
            description += " are more deadly with it";

            if (launcher)
            {
                description += ", and it is most deadly when used with ";
                description += racial_description_string(item);
                description += "ammunition";
            }

            description += ".";
        }
    }

    if (!is_known_artefact(item))
    {
        if (item_ident( item, ISFLAG_KNOW_PLUSES )
            && item.plus >= MAX_WPN_ENCHANT && item.plus2 >= MAX_WPN_ENCHANT)
        {
            description += "$It is maximally enchanted.";
        }
        else
        {
            description += "$It can be maximally enchanted to +";
            _append_value(description, MAX_WPN_ENCHANT, false);
            description += ", +";
            _append_value(description, MAX_WPN_ENCHANT, false);
            description += ".";
        }
    }

    return (description);
}


//---------------------------------------------------------------
//
// describe_ammo
//
//---------------------------------------------------------------
static std::string _describe_ammo( const item_def &item )
{
    std::string description;

    description.reserve(64);

    if (item.sub_type == MI_THROWING_NET)
    {
        if (item.plus > 1 || item.plus < 0)
        {
            std::string how;

            if (item.plus > 1)
                how = "brand-new";
            else if (item.plus < 0)
            {
                if (item.plus > -3)
                    how = "a little worn";
                else if (item.plus > -5)
                    how = "slightly damaged";
                else if (item.plus > -7)
                    how = "damaged";
                else
                    how = "heavily frayed";
            }

            description += "It looks ";
            description += how;
            description += ".";
        }
    }

    bool can_launch       = has_launcher(item);
    bool can_throw        = is_throwable(&you, item, true);
    bool need_new_line    = true;
    bool always_destroyed = false;

    if (item.special && item_type_known(item))
    {
        description += "$$";
        std::string bolt_name;

        std::string threw_or_fired;
        if (can_throw)
        {
            threw_or_fired += "threw";
            if (can_launch)
                threw_or_fired += " or ";
        }
        if (can_launch)
            threw_or_fired += "fired";

        switch (item.special)
        {
        case SPMSL_FLAME:
            bolt_name = "flame";
            // Intentional fall-through
        case SPMSL_FROST:
            if (bolt_name.empty())
                bolt_name = "frost";
            // Intentional fall-through
        case SPMSL_CHAOS:
            if (bolt_name.empty())
                bolt_name = "a random type";

            description += "When ";

            if (can_throw)
            {
                description += "thrown, ";
                if (can_launch)
                    description += "or ";
            }

            if (can_launch)
                description += "fired from an appropriate launcher, ";

            description += "it turns into a bolt of ";
            description += bolt_name;
            description += ".";

            always_destroyed = true;
            break;
        case SPMSL_POISONED:
        case SPMSL_POISONED_II:
            description += "It is coated with poison.";
            break;
        case SPMSL_CURARE:
            description += "It is tipped with asphyxiating poison.";
            break;
        case SPMSL_RETURNING:
            description += "A skilled user can throw it in such a way "
                "that it will return to its owner.";
            break;
        case SPMSL_SHADOW:
            description += "If it kills a monster, causing it to leave a "
                "corpse, the corpse will be animated as a zombie friendly "
                "to the one who " + threw_or_fired + " it.";
            break;
        case SPMSL_PENETRATION:
            description += "It will pass through any targets it hits, "
                "potentially hitting all targets in its path until it "
                "reaches maximum range.";
            break;
        case SPMSL_DISPERSAL:
            description += "Any target it hits will blink, with a "
                "tendancy towards blinking further away from the one who "
                + threw_or_fired + " it.";
            break;
        case SPMSL_EXPLODING:
            description += "It will explode into fragemnets upon hitting "
                "a target, hitting an obstruction, or reaching the end of "
                "its range.";
            always_destroyed = true;
            break;
        case SPMSL_STEEL:
            description += "Compared to normal ammo it does 50% more damage, "
                "is destroyed only 1/10th upon impact, and weighs "
                "three times as much.";
            break;
        case SPMSL_SILVER:
            description += "Compared to normal ammo it does twice as much "
                "damage to the undead, demons and shapeshifters, and "
                "weighs twice as much.";
            break;
        }

        need_new_line = false;
    }

    if (get_equip_race(item) != ISFLAG_NO_RACE)
    {
        description += "$";

        if (need_new_line)
            description += "$";

        if (can_throw)
        {
            unsigned long race = get_equip_race(item);

            description += "It is more deadly when thrown by ";
            description += (race == ISFLAG_DWARVEN) ? "dwarves" :
                           (race == ISFLAG_ELVEN)   ? "elves"
                                                    : "orcs";
            description += (can_launch) ? ", and it" : ".";
            description += " ";
        }

        if (can_launch)
        {
            if (!can_throw)
                description += "It ";

            description += "is more effective in conjunction with ";
            description += racial_description_string(item);
            description += "launchers.";
        }
    }

    if (always_destroyed)
        description += "$It will always be destroyed upon impact.";
    else
        append_missile_info(description);

    if (item_ident( item, ISFLAG_KNOW_PLUSES ) && item.plus >= MAX_WPN_ENCHANT)
        description += "$It is maximally enchanted.";
    else
    {
        description += "$It can be maximally enchanted to +";
        _append_value(description, MAX_WPN_ENCHANT, false);
        description += ".";
    }

    return (description);
}

void append_armour_stats(std::string &description, const item_def &item)
{
    description += "$Armour rating: ";
    _append_value(description, property( item, PARM_AC ), false);
    description += "       ";

    description += "Evasion modifier: ";
    _append_value(description, property( item, PARM_EVASION ), true);
}

void append_missile_info(std::string &description)
{
    description += "$All pieces of ammunition may get destroyed upon impact. "
                   "Enchantment reduces the chances of such loss.";
}

//---------------------------------------------------------------
//
// describe_armour
//
//---------------------------------------------------------------
static std::string _describe_armour( const item_def &item, bool verbose )
{
    std::string description;

    description.reserve(200);

    if (verbose
        && item.sub_type != ARM_SHIELD
        && item.sub_type != ARM_BUCKLER
        && item.sub_type != ARM_LARGE_SHIELD)
    {
        append_armour_stats(description, item);
    }

    const int ego = get_armour_ego_type( item );
    if (ego != SPARM_NORMAL && item_type_known(item) && verbose)
    {
        description += "$$";

        switch (ego)
        {
        case SPARM_RUNNING:
            description += "It allows its wearer to run at a great speed.";
            break;
        case SPARM_FIRE_RESISTANCE:
            description += "It protects its wearer from heat and fire.";
            break;
        case SPARM_COLD_RESISTANCE:
            description += "It protects its wearer from cold.";
            break;
        case SPARM_POISON_RESISTANCE:
            description += "It protects its wearer from poison.";
            break;
        case SPARM_SEE_INVISIBLE:
            description += "It allows its wearer to see invisible things.";
            break;
        case SPARM_DARKNESS:
            description += "When activated it hides its wearer from "
                "the sight of others, but also increases "
                "their metabolic rate by a large amount.";
            break;
        case SPARM_STRENGTH:
            description += "It increases the physical power of its wearer (+3 to strength).";
            break;
        case SPARM_DEXTERITY:
            description += "It increases the dexterity of its wearer (+3 to dexterity).";
            break;
        case SPARM_INTELLIGENCE:
            description += "It makes you more clever (+3 to intelligence).";
            break;
        case SPARM_PONDEROUSNESS:
            description += "It is very cumbersome (-2 to EV, slows movement).";
            break;
        case SPARM_LEVITATION:
            description += "It can be activated to allow its wearer to "
                "float above the ground and remain so indefinitely.";
            break;
        case SPARM_MAGIC_RESISTANCE:
            description += "It increases its wearer's resistance "
                "to enchantments.";
            break;
        case SPARM_PROTECTION:
            description += "It protects its wearer from harm (+3 to AC).";
            break;
        case SPARM_STEALTH:
            description += "It enhances the stealth of its wearer.";
            break;
        case SPARM_RESISTANCE:
            description += "It protects its wearer from the effects "
                "of both cold and heat.";
            break;

        // These two are only for robes.
        case SPARM_POSITIVE_ENERGY:
            description += "It protects its wearer from "
                "the effects of negative energy.";
            break;
        case SPARM_ARCHMAGI:
            description += "It greatly increases the power of its "
                "wearer's magical spells, but is only "
                "intended for those who have very little left to learn.";
            break;

        case SPARM_PRESERVATION:
            description += "It protects its wearer's possessions "
                "from damage and destruction.";
            break;

        case SPARM_REFLECTION:
            description += "It reflects blocked things back in the "
                "direction they came from.";
            break;
        }
    }

    if (is_random_artefact( item ))
    {
        std::string rand_desc = _randart_descrip( item );
        if (!rand_desc.empty())
        {
            description += "$$";
            description += rand_desc;
        }

        // Can't happen, right? (XXX)
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES) && item_type_known(item))
            description += "$This armour may have some hidden properties.";
    }
    else if (get_equip_race( item ) != ISFLAG_NO_RACE)
    {
        // Randart armour can't be racial.
        description += "$";

        unsigned long race = get_equip_race(item);

        if (race == ISFLAG_DWARVEN)
            description += "$It is well-crafted and very durable.";
        else if (race == ISFLAG_ELVEN)
        {
            description += "$It is well-crafted and unobstructive";
            if (item.sub_type == ARM_CLOAK || item.sub_type == ARM_BOOTS)
                description += ", and helps its wearer avoid being noticed";
            description += ".";
        }

        description += "$It fits ";
        description += (race == ISFLAG_DWARVEN) ? "dwarves" :
                       (race == ISFLAG_ELVEN)   ? "elves"
                                                : "orcs";
        description += " well.";
    }

    if (verbose && get_armour_slot(item) == EQ_BODY_ARMOUR)
    {
        description += "$$";
        if (is_light_armour(item))
        {
            description += "This is a light armour. Wearing it will "
                "exercise Dodging and Stealth.";
        }
        else
        {
            description += "This is a heavy armour. Wearing it will "
                "exercise Armour.";
        }
    }

    if (!is_known_artefact(item))
    {
        const int max_ench = armour_max_enchant(item);
        if (item.plus < max_ench || !item_ident( item, ISFLAG_KNOW_PLUSES ))
        {
            description += "$It can be maximally enchanted to +";
            _append_value(description, max_ench, false);
            description += ".";
        }
        else
            description += "$It is maximally enchanted.";
    }

    return description;
}

//---------------------------------------------------------------
//
// describe_jewellery
//
//---------------------------------------------------------------
static std::string _describe_jewellery( const item_def &item, bool verbose)
{
    std::string description;

    description.reserve(200);

    if ((verbose || is_random_artefact( item ))
        && item_ident( item, ISFLAG_KNOW_PLUSES ))
    {
        // Explicit description of ring power (useful for randarts)
        // Note that for randarts we'll print out the pluses even
        // in the case that its zero, just to avoid confusion. -- bwr
        if (item.plus != 0
            || item.sub_type == RING_SLAYING && item.plus2 != 0
            || is_random_artefact( item ))
        {
            switch (item.sub_type)
            {
            case RING_PROTECTION:
                description += "$It affects your AC (";
                _append_value( description, item.plus, true );
                description += ").";
                break;

            case RING_EVASION:
                description += "$It affects your evasion (";
                _append_value( description, item.plus, true );
                description += ").";
                break;

            case RING_STRENGTH:
                description += "$It affects your strength (";
                _append_value( description, item.plus, true );
                description += ").";
                break;

            case RING_INTELLIGENCE:
                description += "$It affects your intelligence (";
                _append_value( description, item.plus, true );
                description += ").";
                break;

            case RING_DEXTERITY:
                description += "$It affects your dexterity (";
                _append_value( description, item.plus, true );
                description += ").";
                break;

            case RING_SLAYING:
                if (item.plus != 0)
                {
                    description += "$It affects your accuracy (";
                    _append_value( description, item.plus, true );
                    description += ").";
                }

                if (item.plus2 != 0)
                {
                    description += "$It affects your damage-dealing abilities (";
                    _append_value( description, item.plus2, true );
                    description += ").";
                }

                if (item.plus == 0 && item.plus2 == 0)
                {
                    description += "This buggy ring affects neither your "
                                   "accuracy nor your damage-dealing "
                                   "abilities.";
                }
                break;

            default:
                break;
            }
        }
    }

    // Randart properties.
    if (is_random_artefact( item ))
    {
        std::string rand_desc = _randart_descrip( item );
        if (!rand_desc.empty())
        {
            description += "$";
            description += rand_desc;
        }
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES))
        {
            description += "$This ";
            description += (jewellery_is_amulet(item) ? "amulet" : "ring");
            description += " may have hidden properties.";
        }
    }

    return (description);
}                               // end describe_jewellery()

static bool _compare_card_names(card_type a, card_type b)
{
    return std::string(card_name(a)) < std::string(card_name(b));
}

//---------------------------------------------------------------
//
// describe_misc_item
//
//---------------------------------------------------------------
static std::string _describe_deck( const item_def &item )
{
    std::string description;

    description.reserve(100);

    description += "$";

    const std::vector<card_type> drawn_cards = get_drawn_cards(item);
    if (!drawn_cards.empty())
    {
        description += "Drawn card(s): ";
        for (unsigned int i = 0; i < drawn_cards.size(); ++i)
        {
            if (i != 0)
                description += ", ";
            description += card_name(drawn_cards[i]);
        }
        description += "$";
    }

    const int num_cards = cards_in_deck(item);
    int last_known_card = -1;
    if (top_card_is_known(item))
    {
        description += "Next card(s): ";
        for (int i = 0; i < num_cards; ++i)
        {
            unsigned char flags;
            const card_type card = get_card_and_flags(item, -i-1, flags);
            if (flags & CFLAG_MARKED)
            {
                if (i != 0)
                    description += ", ";
                description += card_name(card);
                last_known_card = i;
            }
            else
                break;
        }
        description += "$";
    }

    // Marked cards which we don't know straight off.
    std::vector<card_type> marked_cards;
    for (int i = last_known_card + 1; i < num_cards; ++i)
    {
        unsigned char flags;
        const card_type card = get_card_and_flags(item, -i-1, flags);
        if (flags & CFLAG_MARKED)
            marked_cards.push_back(card);
    }
    if (!marked_cards.empty())
    {
        std::sort(marked_cards.begin(), marked_cards.end(),
                  _compare_card_names);

        description += "Marked card(s): ";
        for (unsigned int i = 0; i < marked_cards.size(); ++i)
        {
            if (i != 0)
                description += ", ";
            description += card_name(marked_cards[i]);
        }
        description += "$";
    }

    // Seen cards in the deck.
    std::vector<card_type> seen_cards;
    for (int i = 0; i < num_cards; ++i)
    {
        unsigned char flags;
        const card_type card = get_card_and_flags(item, -i-1, flags);

        // This *might* leak a bit of information...oh well.
        if ((flags & CFLAG_SEEN) && !(flags & CFLAG_MARKED))
            seen_cards.push_back(card);
    }
    if (!seen_cards.empty())
    {
        std::sort(seen_cards.begin(), seen_cards.end(),
                  _compare_card_names);

        description += "Seen card(s): ";
        for (unsigned int i = 0; i < seen_cards.size(); ++i)
        {
            if (i != 0)
                description += ", ";
            description += card_name(seen_cards[i]);
        }
        description += "$";
    }

    return (description);
}

// Adds a list of all spells contained in a book or rod to its
// description string.
void append_spells(std::string &desc, const item_def &item)
{
    if (!item.has_spells())
        return;

    desc += "$$Spells                             Type                      Level$";

    for (int j = 0; j < 8; j++)
    {
        spell_type stype = which_spell_in_book(item, j);
        if (stype == SPELL_NO_SPELL)
            continue;

        std::string name = (is_memorised(stype)) ? "*" : "";
                    name += spell_title(stype);
        desc += name;
        for (unsigned int i = 0; i < 35 - name.length(); i++)
             desc += " ";

        name = "";
        if (item.base_type == OBJ_STAVES)
            name += "Evocations";
        else
        {
            bool already = false;

            for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
            {
                if (spell_typematch( stype, 1 << i ))
                {
                    if (already)
                        name += "/" ;

                    name += spelltype_name( 1 << i );
                    already = true;
                }
            }
        }
        desc += name;

        for (unsigned int i = 36; i < 65 - name.length(); i++)
             desc += " ";

        char sval[3];
        itoa( spell_difficulty( stype ), sval, 10 );
        desc += sval;
        desc += "$";
    }
}

// ========================================================================
//      Public Functions
// ========================================================================

bool is_dumpable_artefact( const item_def &item, bool verbose)
{
    bool ret = false;

    if (is_known_artefact( item ))
    {
        ret = item_ident( item, ISFLAG_KNOW_PROPERTIES );
    }
    else if (verbose && item.base_type == OBJ_ARMOUR
             && item_type_known(item))
    {
        const int spec_ench = get_armour_ego_type( item );
        ret = (spec_ench >= SPARM_RUNNING && spec_ench <= SPARM_PRESERVATION);
    }
    else if (verbose && item.base_type == OBJ_JEWELLERY
             && item_type_known(item))
    {
        ret = true;
    }

    return (ret);
}


//---------------------------------------------------------------
//
// get_item_description
//
// Note that the string will include dollar signs which should
// be interpreted as carriage returns.
//
//---------------------------------------------------------------
std::string get_item_description( const item_def &item, bool verbose,
                                  bool dump, bool noquote )
{
    if (dump)
        noquote = true;

    std::ostringstream description;

    if (!dump)
        description << item.name(DESC_INVENTORY_EQUIP);

#if DEBUG_DIAGNOSTICS
    if (!dump)
    {
        description << std::setfill('0');
        description << "$$"
                    << "base: " << static_cast<int>(item.base_type)
                    << " sub: " << static_cast<int>(item.sub_type)
                    << " plus: " << item.plus << " plus2: " << item.plus2
                    << " special: " << item.special
                    << "$"
                    << "quant: " << item.quantity
                    << " colour: " << static_cast<int>(item.colour)
                    << " flags: " << std::hex << std::setw(8) << item.flags
                    << std::dec << "$"
                    << "x: " << item.pos.x << " y: " << item.pos.y
                    << " link: " << item.link
                    << " slot: " << item.slot
                    << " ident_type: "
                    << static_cast<int>(get_ident_type(item));
    }
#endif

    if (is_unrandom_artefact( item )
        && strlen(unrandart_descrip(1, item)) != 0)
    {
        description << "$$";
        description << unrandart_descrip(1, item);
        description << "$";
    }

    if (is_fixed_artefact(item) && item_type_known(item))
    {
        // Known fixed artifacts are handled elsewhere.
    }
    else if (verbose && is_fixed_artefact(item))
    {
        description << "$$";
        description << article_a(item.name(DESC_CAP_A, true,
                                           false, false), false);
        description << ".$";
    }
    else if (verbose || (item.base_type != OBJ_WEAPONS
                         && item.base_type != OBJ_ARMOUR
                         && item.base_type != OBJ_BOOKS))
    {
        description << "$$";

        if (dump)
        {
            description << "["
                        << item.name(DESC_DBNAME, true, false, false)
                        << "]";
        }
        else
        {
            std::string db_name = item.name(DESC_DBNAME, true, false, false);
            std::string db_desc = getLongDescription(db_name);
            if (!noquote && !is_known_artefact(item))
            {
                const unsigned int lineWidth = get_number_of_cols();
                const          int height    = get_number_of_lines();

                std::string quote = getQuoteString(db_name);

                if (_count_desc_lines(db_desc, lineWidth)
                    + _count_desc_lines(quote, lineWidth) <= height)
                {
                    if (!db_desc.empty())
                        db_desc += "$";
                    db_desc += quote;
                }
            }

            if (db_desc.empty())
            {
                if (item_type_known(item))
                {
                    description << "[ERROR: no desc for item name '" << db_name
                                << "']$";
                }
                else
                {
                    description << article_a(item.name(DESC_CAP_A, true,
                                                       false, false), false);
                    description << ".$";
                }
            }
            else
                description << db_desc;

            if (item.base_type == OBJ_WANDS
                || item.base_type == OBJ_MISSILES
                || item.base_type == OBJ_FOOD && item.sub_type == FOOD_CHUNK)
            {
                // Get rid of newline at end of description, so that
                // either the wand "no charges left" or the meat chunk
                // "unpleasant" description can follow on the same line.
                // Same for missiles' descriptions.
                description.seekp(description.tellp() - (std::streamoff)1);
                description << " ";
            }
        }
    }

    bool need_extra_line = true;
    std::string desc;
    switch (item.base_type)
    {
    // Weapons, armour, jewellery, books might be artefacts.
    case OBJ_WEAPONS:
        desc = _describe_weapon( item, verbose );
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_ARMOUR:
        desc = _describe_armour( item, verbose );
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_JEWELLERY:
        desc = _describe_jewellery( item, verbose );
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_BOOKS:
        if (!player_can_read_spellbook( item ))
        {
            description << "$This book is beyond your current level of "
                           "understanding.";
        }
        else if (!verbose && is_random_artefact( item ))
        {
            append_spells( desc, item );
            if (desc.empty())
                need_extra_line = false;
            else
                description << desc;
        }
        break;

    case OBJ_MISSILES:
        description << _describe_ammo( item );
        break;

    case OBJ_WANDS:
        if (item_type_known(item))
        {
            const int max_charges = 3 * wand_charge_value(item.sub_type);
            if (item.plus < max_charges
                || !item_ident(item, ISFLAG_KNOW_PLUSES))
            {
                description << "$It can have at most " << max_charges
                            << " charges.";
            }
            else
                description << "$It is fully charged.";
        }

        if (item_ident( item, ISFLAG_KNOW_PLUSES ) && item.plus == 0
            || item.plus2 == ZAPCOUNT_EMPTY)
        {
            description << "$Unfortunately, it has no charges left.";
        }
        description << "$";
        break;

    case OBJ_CORPSES:
        if (item.sub_type == CORPSE_SKELETON)
            break;
        // intentional fall-through
    case OBJ_FOOD:
        if (item.base_type == OBJ_CORPSES || item.sub_type == FOOD_CHUNK)
        {
            if (food_is_rotten(item))
            {
                if (player_mutation_level(MUT_SAPROVOROUS) == 3)
                    description << "It looks nice and ripe.";
                else
                {
                    description << "In fact, it is rotting away before your "
                                   "eyes.";

                    if (!you.is_undead
                        && !player_mutation_level(MUT_SAPROVOROUS))
                    {
                        description << " Eating it is completely out of the "
                                       "question!";
                    }
                }
            }
            else if (player_mutation_level(MUT_SAPROVOROUS) < 3)
                description << "It looks rather unpleasant.";

            switch (mons_corpse_effect(item.plus))
            {
            case CE_POISONOUS:
                description << "$$This meat is poisonous.";
                break;
            case CE_MUTAGEN_RANDOM:
                if (you.species != SP_GHOUL)
                {
                    description << "$$Eating this meat will cause random "
                                   "mutations.";
                }
                break;
            case CE_HCL:
                if (you.species != SP_GHOUL)
                    description << "$$Eating this meat will cause rotting.";
                break;
            case CE_CONTAMINATED:
                if (player_mutation_level(MUT_SAPROVOROUS) < 3)
                {
                    description << "$$Meat like this may occasionally cause "
                                   "sickness.";
                }
                break;
            default:
                break;
            }

            if (is_good_god(you.religion) && is_player_same_species(item.plus)
                || you.religion == GOD_ZIN
                   && mons_class_intel(item.plus) >= I_NORMAL)
            {
                description << "$$" << god_name(you.religion) << " disapproves "
                               "of eating such meat.";
            }
            description << "$";
        }
        break;

    case OBJ_STAVES:
        if (item_is_rod( item ))
        {
            description <<
                "$It uses its own mana reservoir for casting spells, and "
                "recharges automatically by channeling mana from its "
                "wielder.";

            const int max_charges = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
            if (item_ident(item, ISFLAG_KNOW_PLUSES)
                && item.plus2 >= max_charges && item.plus >= item.plus2)
            {
                description << "$It is fully charged.";
            }
            else
            {
                description << "$It can have at most " << max_charges
                            << " charges.";
            }
        }
        else
        {
            description <<
                "$Damage rating: 7    Accuracy rating: +6    "
                "Attack delay: 120%";

            description << "$$It falls into the 'staves' category.";
        }
        break;

    case OBJ_MISCELLANY:
        if (is_deck(item))
            description << _describe_deck( item );
        break;

    case OBJ_POTIONS:
#ifdef DEBUG_BLOOD_POTIONS
        // List content of timer vector for blood potions.
        if (is_blood_potion(item))
        {
            item_def stack = static_cast<item_def>(item);
            CrawlHashTable &props = stack.props;
            ASSERT(props.exists("timer"));
            CrawlVector &timer = props["timer"].get_vector();
            ASSERT(!timer.empty());

            description << "$Quantity: " << stack.quantity
                        << "        Timer size: " << (int) timer.size();
            description << "$Timers:$";
            for (int i = 0; i < timer.size(); i++)
                 description << (timer[i].get_long()) << "  ";
        }
#endif
        break;

    case OBJ_SCROLLS:
    case OBJ_ORBS:
    case OBJ_GOLD:
        // No extra processing needed for these item types.
        break;

    default:
        DEBUGSTR("Bad item class");
        description << "$This item should not exist. Mayday! Mayday!";
    }

    if (!verbose && item_known_cursed( item ))
        description << "$It has a curse placed upon it.";
    else
    {
        if (verbose)
        {
            if (need_extra_line)
                description << "$";
            description << "$It";
            if (item_known_cursed( item ))
                description << " has a curse placed upon it, and it";

            const int mass = item_mass( item );
            description << " weighs around " << (mass / 10)
                        << "." << (mass % 10)
                        << " aum. "; // arbitrary unit of mass

            if (is_dumpable_artefact(item, false))
            {
                if (item.base_type == OBJ_ARMOUR
                    || item.base_type == OBJ_WEAPONS)
                {
                    description << "$$This ancient artefact cannot be changed "
                        "by magic or mundane means.";
                }
                else
                {
                    description << "$$It is an ancient artefact.";
                }
            }
        }
    }

    if (good_god_dislikes_item_handling(item))
    {
        description << "$$" << god_name(you.religion) << " opposes the use of "
                    << "such an evil item.";
    }
    else if (god_dislikes_item_handling(item))
    {
        description << "$$" << god_name(you.religion) << " disapproves of the "
                    << "use of such an item.";
    }

    return description.str();
}                               // end get_item_description()

static std::string _marker_feature_description(const coord_def &pos)
{
    std::vector<map_marker*> markers = env.markers.get_markers_at(pos);
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        const std::string desc = markers[i]->feature_description_long();
        if (!desc.empty())
            return (desc);
    }
    return ("");
}

static std::string _get_feature_description_wide(int feat)
{
    return std::string();
}

void describe_feature_wide(int x, int y)
{
    const coord_def pos(x, y);
    const dungeon_feature_type feat = grd(pos);

    std::string desc = feature_description(pos, false, DESC_CAP_A, false);
    std::string db_name =
        grd[x][y] == DNGN_ENTER_SHOP ? "A shop" : desc;
    std::string long_desc = getLongDescription(db_name);

    desc += ".$$";

    // If we couldn't find a description in the database then see if
    // the feature's base name is different.
    if (long_desc.empty())
    {
        db_name   = feature_description(pos, false, DESC_CAP_A, false, true);
        long_desc = getLongDescription(db_name);
    }

    bool custom_desc = false;

    if (feat == DNGN_ENTER_PORTAL_VAULT)
    {
        std::string _desc = _marker_feature_description(pos);
        if (!_desc.empty())
        {
            long_desc   = _desc;
            custom_desc = true;
        }
    }

    const CrawlHashTable &props = env.properties;
    if (!custom_desc && props.exists(LONG_DESC_KEY))
    {
        const CrawlHashTable &desc_table = props[LONG_DESC_KEY].get_table();

        // First try the modified name, then the base name.
        std::string key = raw_feature_description(feat);
        if (!desc_table.exists(key))
            key = raw_feature_description(feat, NUM_TRAPS, true);

        if (desc_table.exists(key))
        {
            long_desc   = desc_table[key].get_string();
            custom_desc = true;
        }

        std::string quote = getQuoteString(key);
        if (!quote.empty())
            db_name = key;
    }

    desc += long_desc;

    // For things which require logic
    if (!custom_desc)
        desc += _get_feature_description_wide(grd[x][y]);

    std::string quote = getQuoteString(db_name);

    print_description(desc, "", "", "", "", quote);

    mouse_control mc(MOUSE_MODE_MORE);

    if (Options.tutorial_left)
        tutorial_describe_pos(x, y);

    if (getch() == 0)
        getch();
}

void set_feature_desc_long(const std::string &raw_name,
                           const std::string &desc)
{
    ASSERT(!raw_name.empty());

    CrawlHashTable &props = env.properties;

    if (!props.exists(LONG_DESC_KEY))
        props[LONG_DESC_KEY].new_table(SV_STR);

    CrawlHashTable &desc_table = props[LONG_DESC_KEY];

    if (desc.empty())
        desc_table.erase(raw_name);
    else
        desc_table[raw_name] = desc;
}

// Returns true if spells can be shown to player.
static bool _show_item_description(const item_def &item)
{
    const unsigned int lineWidth = get_number_of_cols()  - 1;
    const          int height    = get_number_of_lines();

    std::string description = get_item_description( item, 1, false,
                                                   Options.tutorial_left);

    int num_lines = _count_desc_lines(description, lineWidth) + 1;

    // XXX: hack: Leave room for "Inscribe item?" and the blank line above
    // it by removing item quote.  This should really be taken care of
    // by putting the quotes into a separate DB and treating them as
    // a suffix that can be ignored by print_description().
    if (height - num_lines <= 2)
        description = get_item_description( item, 1, false, true);

    print_description(description);
    if (Options.tutorial_left)
        tutorial_describe_item(item);

    if (item.has_spells())
    {
        if (item.base_type == OBJ_BOOKS && !player_can_read_spellbook( item ))
            return (false);

        formatted_string fs;
        item_def dup = item;
        spellbook_contents( dup,
                item.base_type == OBJ_BOOKS?
                    RBOOK_READ_SPELL
                  : RBOOK_USE_STAFF,
                &fs );
        fs.display(2, -2);
        return (true);
    }

    return (false);
}

static bool _describe_spells(const item_def &item)
{
    int c = getch();
    if (c < 'a' || c > 'h')     //jmf: was 'g', but 8=h
    {
        mesclr( true );
        return (false);
    }

    const int spell_index = letter_to_index(c);

    spell_type nthing =
        which_spell_in_book(item, spell_index);
    if (nthing == SPELL_NO_SPELL)
        return (false);

    describe_spell( nthing );
    return (true);
}

//---------------------------------------------------------------
//
// describe_item
//
// Describes all items in the game.
//
//---------------------------------------------------------------
void describe_item( item_def &item, bool allow_inscribe )
{
    while (true)
    {
        const bool spells_shown = _show_item_description(item);

        if (spells_shown)
        {
            cgotoxy(1, wherey());
            textcolor(LIGHTGREY);
            cprintf("Select a spell to read its description.");
            if (_describe_spells(item))
                continue;
            return;
        }
        break;
    }

    // Don't ask if there aren't enough rows left
    if (allow_inscribe && wherey() <= get_number_of_lines() - 3)
    {
        cgotoxy(1, wherey() + 2);
        inscribe_item(item, false);
    }
    else if (getch() == 0)
        getch();
}

// There are currently two ways to inscribe an item:
// * using the inscribe command ('{') -> proper_prompt = true
// * from the inventory when viewing an item -> proper_prompt = false
//
// Thus, proper_prompt also controls whether a tutorial explanation can be
// shown, or whether the pre- and post-inscription item names need to be
// printed.
void inscribe_item(item_def &item, bool proper_prompt)
{
    if (proper_prompt)
        mpr(item.name(DESC_INVENTORY).c_str(), MSGCH_EQUIPMENT);

    const bool is_inscribed = !item.inscription.empty();
    std::string ainscrip;

    // Only allow autoinscription if we don't have all the text
    // already.
    bool need_autoinscribe = false;
    if (is_random_artefact(item))
    {
        ainscrip = randart_auto_inscription(item);
        if (!ainscrip.empty()
            && (!is_inscribed
                || item.inscription.find(ainscrip) == std::string::npos))
        {
            need_autoinscribe = true;
        }
    }

    std::string prompt;
    int keyin;
    bool did_prompt = false;

    // Don't prompt for whether to inscribe in the first place if the
    // player is using '{' - unless autoinscribing or clearing an
    // existing inscription become an option.
    if (!proper_prompt || need_autoinscribe || is_inscribed)
    {
        prompt = "Press (i) to ";
        prompt += (is_inscribed ? "add to inscription"
                                : "inscribe");

        if (need_autoinscribe || is_inscribed)
        {
            if (!need_autoinscribe || !is_inscribed)
                prompt += " or ";
            else
                prompt += ", ";

            if (need_autoinscribe)
            {
                prompt += "(a) to autoinscribe";
                if (is_inscribed)
                    prompt += ", or ";
            }
            if (is_inscribed)
                prompt += "(c) to clear it";
        }
        prompt += ". ";

        if (proper_prompt)
            mpr(prompt.c_str(), MSGCH_PROMPT);
        else
        {
            prompt = "<cyan>" + prompt + "</cyan>";
            formatted_string::parse_string(prompt).display();

            if (Options.tutorial_left && wherey() <= get_number_of_lines() - 5)
                tutorial_inscription_info(need_autoinscribe, prompt);
        }
        did_prompt = true;
    }

    keyin = (did_prompt ? tolower(c_getch()) : 'i');
    switch (keyin)
    {
    case 'c':
        item.inscription.clear();
        break;
    case 'a':
        if (need_autoinscribe)
        {
            add_autoinscription(item, ainscrip);
            break;
        }
        // If autoinscription is impossible, prompt for an inscription instead.
    case 'i':
    {
        prompt = (is_inscribed ? "Add what to inscription? "
                               : "Inscribe with what? ");

        if (proper_prompt)
            mpr(prompt.c_str(), MSGCH_PROMPT);
        else
        {
            prompt = EOL "<cyan>" + prompt + "</cyan>";
            formatted_string::parse_string(prompt).display();
        }

        char buf[79];
        if (!cancelable_get_line(buf, sizeof buf))
        {
            // Strip spaces from the end.
            for (int i = strlen(buf) - 1; i >= 0; i--)
            {
                if (isspace( buf[i] ))
                    buf[i] = 0;
                else
                    break;
            }

            if (strlen(buf) > 0)
            {
                if (is_inscribed)
                    item.inscription += ", ";

                item.inscription += std::string(buf);
            }
        }
        else if (proper_prompt)
        {
            canned_msg(MSG_OK);
            return;
        }
        break;
    }
    default:
        if (proper_prompt)
            canned_msg(MSG_OK);
        return;
    }

    if (proper_prompt)
    {
        mpr(item.name(DESC_INVENTORY).c_str(), MSGCH_EQUIPMENT);
        you.wield_change  = true;
    }

}
//---------------------------------------------------------------
//
// describe_spell
//
// Describes (most) every spell in the game.
//
//---------------------------------------------------------------
void describe_spell(spell_type spelled)
{
    std::string description;

    description.reserve(500);

    description += spell_title( spelled );
    description += "$$";
    const std::string long_descrip = getLongDescription(spell_title(spelled));
    if (!long_descrip.empty())
        description += long_descrip;
    else
    {
        description += "This spell has no description. "
                       "Casting it may therefore be unwise. "
#if DEBUG
                       "Instead, go fix it. ";
#else
                       "Please file a bug report.";
#endif
    }

    print_description(description);

    mouse_control mc(MOUSE_MODE_MORE);

    if (getch() == 0)
        getch();
}

static std::string _describe_draconian_role(const monsters *mon)
{
    switch (mon->type)
    {
    case MONS_DRACONIAN_SHIFTER:
        return "It darts around disconcertingly without taking a step.";
    case MONS_DRACONIAN_SCORCHER:
        return "Its scales are sooty black from years of magical pyrotechnics.";
    case MONS_DRACONIAN_ZEALOT:
        return "In its gaze you see all the malefic power of its "
               "terrible god.";
    case MONS_DRACONIAN_ANNIHILATOR:
        return "Crackling balls of pure energy hum and spark up and down its "
               "scaled fists and arms.";
    case MONS_DRACONIAN_CALLER:
        return "It looks especially reptilian, and eager for company.";
    case MONS_DRACONIAN_MONK:
        return "It looks unnaturally strong and dangerous with its fists.";
    case MONS_DRACONIAN_KNIGHT:
        return "It wields a deadly weapon with menacing efficiency.";
    default:
        return ("");
    }
}

static std::string _describe_draconian_colour(int species)
{
    switch (species)
    {
    case MONS_BLACK_DRACONIAN:
        return "Sparks crackle and flare out of its mouth and nostrils.";
    case MONS_MOTTLED_DRACONIAN:
        return "Liquid flames drip from its mouth.";
    case MONS_YELLOW_DRACONIAN:
        return "Acid fumes swirl around it.";
    case MONS_GREEN_DRACONIAN:
        return "Venom steams and drips from its jaws.";
    case MONS_PURPLE_DRACONIAN:
        return "Its outline shimmers with wild energies.";
    case MONS_RED_DRACONIAN:
        return "Smoke pours from its nostrils.";
    case MONS_WHITE_DRACONIAN:
        return "Frost pours from its nostrils.";
    case MONS_PALE_DRACONIAN:
        return "It is cloaked in a pall of superheated steam.";
    }
    return ("");
}

static std::string _describe_draconian(const monsters *mon)
{
    std::string description;
    const int subsp = draco_subspecies( mon );

    if (subsp == MONS_DRACONIAN)
        description += "A ";
    else
        description += "A muscular ";

    switch (subsp)
    {
    case MONS_DRACONIAN:            description += "brown-";   break;
    case MONS_BLACK_DRACONIAN:      description += "black-";   break;
    case MONS_MOTTLED_DRACONIAN:    description += "mottled-"; break;
    case MONS_YELLOW_DRACONIAN:     description += "yellow-";  break;
    case MONS_GREEN_DRACONIAN:      description += "green-";   break;
    case MONS_PURPLE_DRACONIAN:     description += "purple-";  break;
    case MONS_RED_DRACONIAN:        description += "red-";     break;
    case MONS_WHITE_DRACONIAN:      description += "white-";   break;
    case MONS_PALE_DRACONIAN:       description += "pale-";    break;
    default:
        break;
    }

    description += "scaled humanoid with wings.";

    if (subsp != MONS_DRACONIAN)
    {
        const std::string drac_col = _describe_draconian_colour(subsp);
        if (!drac_col.empty())
            description += "  " + drac_col;
    }

    if (subsp != mon->type)
    {
        const std::string drac_role = _describe_draconian_role(mon);
        if (!drac_role.empty())
            description += "  " + drac_role;
    }

    return (description);
}

static const char* _get_resist_name(mon_resist_flags res_type)
{
    switch (res_type)
    {
    case MR_RES_ELEC:
        return "electricity";
    case MR_RES_POISON:
        return "poison";
    case MR_RES_FIRE:
        return "fire";
    case MR_RES_STEAM:
        return "steam";
    case MR_RES_COLD:
        return "cold";
    case MR_RES_ACID:
        return "acid";
    default:
        return "buggy resistance";
    }
}

// Describe a monster's (intrinsic) resistances, speed and a few other
// attributes.
static std::string _monster_stat_description(const monsters& mon)
{
    const mon_resist_def resist = get_mons_resists(&mon);
    const mon_resist_flags resists[] = {
        MR_RES_ELEC,  MR_RES_POISON, MR_RES_FIRE,
        MR_RES_STEAM, MR_RES_COLD,   MR_RES_ACID
    };

    std::vector<std::string> extreme_resists;
    std::vector<std::string> high_resists;
    std::vector<std::string> base_resists;
    std::vector<std::string> suscept;

    for (unsigned int i = 0; i < ARRAYSZ(resists); ++i)
    {
        int level = resist.get_resist_level(resists[i]);
        if (level != 0)
        {
            const char* attackname = _get_resist_name(resists[i]);
            level = std::max(level, -1);
            level = std::min(level,  3);
            switch (level)
            {
                case -1:
                    suscept.push_back(attackname);
                    break;
                case 1:
                    base_resists.push_back(attackname);
                    break;
                case 2:
                    high_resists.push_back(attackname);
                    break;
                case 3:
                    extreme_resists.push_back(attackname);
                    break;
            }
        }
    }

    const char* pronoun = mons_pronoun(static_cast<monster_type>(mon.type),
                                       PRONOUN_CAP, true);

    std::vector<std::string> resist_descriptions;
    if (!extreme_resists.empty())
    {
        const std::string tmp = "extremely resistant to "
            + comma_separated_line(extreme_resists.begin(),
                                   extreme_resists.end());
        resist_descriptions.push_back(tmp);
    }
    if (!high_resists.empty())
    {
        const std::string tmp = "very resistant to "
            + comma_separated_line(high_resists.begin(), high_resists.end());
        resist_descriptions.push_back(tmp);
    }
    if (!base_resists.empty())
    {
        const std::string tmp = "resistant to "
            + comma_separated_line(base_resists.begin(), base_resists.end());
        resist_descriptions.push_back(tmp);
    }

    std::ostringstream result;

    if (!resist_descriptions.empty())
    {
        result << pronoun << " is "
               << comma_separated_line(resist_descriptions.begin(),
                                       resist_descriptions.end(),
                                       "; and ", "; ")
               << ".$";
    }

    // Is monster susceptible to anything? (On a new line.)
    if (!suscept.empty())
    {
        result << pronoun << " is susceptible to "
               << comma_separated_line(suscept.begin(), suscept.end())
               << ".$";
    }

    // Magic resistance at MAG_IMMUNE.
    if (mons_immune_magic(&mon))
        result << pronoun << " is immune to magical enchantments.$";

    // Seeing/sensing invisible.
    if (mons_class_flag(mon.type, M_SEE_INVIS))
        result << pronoun << " can see invisible.$";
    else if (mons_class_flag(mon.type, M_SENSE_INVIS))
        result << pronoun << " can sense the presence of invisible creatures.$";

    // Unusual monster speed.
    const int speed = mons_speed(mon.type);
    if (speed != 10)
    {
        result << pronoun << " is ";
        if (speed < 7)
            result << "very slow";
        else if (speed < 10)
            result << "slow";
        else if (speed > 20)
            result << "extremely fast";
        else if (speed > 15)
            result << "very fast";
        else if (speed > 10)
            result << "fast";
        result << ".$";
    }

    // Can the monster fly/levitate?
    const flight_type fly = mons_class_flies(mon.type);
    if (fly != FL_NONE)
    {
        result << pronoun << " can "
               << (fly == FL_FLY ? "fly" : "levitate") << ".$";
    }

    return result.str();
}

void describe_monsters(const monsters& mons)
{
    // For undetected mimics describe mimicked item instead.
    if (mons_is_mimic(mons.type) && !(mons.flags & MF_KNOWN_MIMIC))
    {
        item_def item;
        get_mimic_item(&mons, item);
        describe_item(item);
        return;
    }

    std::ostringstream body;
    std::string title, prefix, suffix, quote;

    title = mons.full_name(DESC_CAP_A, true);

    std::string db_name = mons.base_name(DESC_DBNAME);
    if (mons_is_mimic(mons.type) && mons.type != MONS_GOLD_MIMIC)
        db_name = "mimic";

    // Don't get description for player ghosts.
    if (mons.type != MONS_PLAYER_GHOST)
        body << getLongDescription(db_name);
    quote = getQuoteString(db_name);

    std::string symbol;
    symbol += get_monster_data(mons.type)->showchar;
    if (isupper(symbol[0]))
        symbol = "cap-" + symbol;

    std::string symbol_prefix = "__";
    symbol_prefix += symbol;
    symbol_prefix += "_prefix";
    prefix = getLongDescription(symbol_prefix);

    std::string quote2 = getQuoteString(symbol_prefix);
    if (!quote.empty() && !quote2.empty())
        quote += "$";
    quote += quote2;

    // Except for draconians and player ghosts, I have to admit I find the
    // following special descriptions rather pointless. I certainly can't
    // say I like them, though "It has come for your soul!" and
    // "It wants to drink your blood!" have something going for them. (jpeg)
    switch (mons.type)
    {
    case MONS_ROTTING_DEVIL:
        if (player_can_smell())
        {
            if (player_mutation_level(MUT_SAPROVOROUS) == 3)
                body << "$It smells great!$";
            else
                body << "$It stinks.$";
        }
        break;

    case MONS_SWAMP_DRAKE:
        if (player_can_smell())
            body << "$It smells horrible.$";
        break;

    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GUARDIAN_NAGA:
    case MONS_GREATER_NAGA:
        if (you.species == SP_NAGA)
            body << "$It is particularly attractive.$";
        else
            body << "$It is strange and repulsive.$";
        break;

    case MONS_VAMPIRE:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAMPIRE_MAGE:
        if (you.is_undead == US_ALIVE)
            body << "$It wants to drink your blood!$";
        break;

    case MONS_REAPER:
        if (you.is_undead == US_ALIVE)
            body <<  "$It has come for your soul!$";
        break;

    case MONS_ELF:
        // These are only possible from polymorphing or shapeshifting.
        body << "$This one is remarkably plain looking.$";
        break;

    case MONS_DRACONIAN:
    case MONS_RED_DRACONIAN:
    case MONS_WHITE_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_PALE_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_KNIGHT:
    {
        body << "$" << _describe_draconian( &mons );
        break;
    }
    case MONS_PLAYER_GHOST:
        body << "The apparition of " << get_ghost_description(mons) << ".$";
        break;

    case MONS_PANDEMONIUM_DEMON:
        body << _describe_demon(mons);
        break;

    case MONS_URUG:
        if (player_can_smell())
            body << "$He smells terrible.$";
        break;

    case MONS_PROGRAM_BUG:
        body << "If this monster is a \"program bug\", then it's "
                "recommended that you save your game and reload.  Please report "
                "monsters who masquerade as program bugs or run around the "
                "dungeon without a proper description to the authorities.$";
        break;
    default:
        break;
    }

    // Don't leak or duplicate resistance information for ghosts/demons.
    if (mons.type != MONS_PANDEMONIUM_DEMON && mons.type != MONS_PLAYER_GHOST)
    {
        std::string result = _monster_stat_description(mons);
        if (!result.empty())
            body << "$" << result;
    }

    if (!mons_can_use_stairs(&mons))
    {
        body << "$" << mons_pronoun(static_cast<monster_type>(mons.type),
                                    PRONOUN_CAP, true)
             << " is incapable of using stairs.$";
    }

    if (mons_is_summoned(&mons))
    {
        body << "$" << "This monster has been summoned, and is thus only "
                       "temporary. Killing it yields no experience, nutrition "
                       "or items.$";
    }

    std::string symbol_suffix = "__";
    symbol_suffix += symbol;
    symbol_suffix += "_suffix";
    suffix += getLongDescription(symbol_suffix);
    suffix += getLongDescription(symbol_suffix + "_examine");

    quote2 = getQuoteString(symbol_suffix);
    if (!quote.empty() && !quote2.empty())
        quote += "$";
    quote += quote2;

#if DEBUG_DIAGNOSTICS
    if (mons_class_flag( mons.type, M_SPELLCASTER ))
    {
        const monster_spells &hspell_pass = mons.spells;
        bool found_spell = false;

        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
        {
            if (hspell_pass[i] != SPELL_NO_SPELL)
            {
                if (!found_spell)
                {
                    body << "$$Monster Spells:$";
                    found_spell = true;
                }

                body << "    " << i << ": "
                     << spell_title(hspell_pass[i])
                     << " (" << static_cast<int>(hspell_pass[i])
                     << ")$";
            }
        }
    }

    bool has_item = false;
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
    {
        if (mons.inv[i] != NON_ITEM)
        {
            if (!has_item)
            {
                body << "$Monster Inventory:$";
                has_item = true;
            }
            body << "    " << i << ") "
                 << mitm[mons.inv[i]].name(DESC_NOCAP_A, false, true);
        }
    }
#endif

    print_description(body.str(), title, suffix, prefix, "", quote);

    mouse_control mc(MOUSE_MODE_MORE);

    if (Options.tutorial_left)
        tutorial_describe_monster(&mons);

    if (getch() == 0)
        getch();
}

// Describes the current ghost's previous owner. The caller must
// prepend "The apparition of" or whatever and append any trailing
// punctuation that's wanted.
std::string get_ghost_description(const monsters &mons, bool concise)
{
    ASSERT(mons.ghost.get());
    std::ostringstream gstr;

    const ghost_demon &ghost = *(mons.ghost);

    const species_type gspecies = ghost.species;

    // We're fudging stats so that unarmed combat gets based off
    // of the ghost's species, not the player's stats... exact
    // stats aren't required anyways, all that matters is whether
    // dex >= str. -- bwr
    const int dex = 10;
    int str;
    switch (gspecies)
    {
    case SP_MOUNTAIN_DWARF:
    case SP_TROLL:
    case SP_OGRE:
    case SP_MINOTAUR:
    case SP_HILL_ORC:
    case SP_CENTAUR:
    case SP_NAGA:
    case SP_MUMMY:
    case SP_GHOUL:
        str = 15;
        break;

    case SP_HUMAN:
    case SP_DEMIGOD:
    case SP_DEMONSPAWN:
        str = 10;
        break;

    default:
        str = 5;
        break;
    }

    gstr << ghost.name << " the "
         << skill_title( ghost.best_skill,
                         (unsigned char)ghost.best_skill_level,
                         gspecies,
                         str, dex, GOD_NO_GOD )
         << ", a"
         << ((ghost.xl <  4) ? " weakling" :
             (ghost.xl <  7) ? "n average" :
             (ghost.xl < 11) ? "n experienced" :
             (ghost.xl < 16) ? " powerful" :
             (ghost.xl < 22) ? " mighty" :
             (ghost.xl < 26) ? " great" :
             (ghost.xl < 27) ? "n awesomely powerful"
                             : " legendary")
         << " ";
    if (concise)
    {
        gstr << get_species_abbrev(gspecies)
             << get_class_abbrev(ghost.job);
    }
    else
    {
        gstr << species_name(gspecies,
                             ghost.xl)
             << " "
             << get_class_name(ghost.job);
    }

    return gstr.str();
}

extern ability_type god_abilities[MAX_NUM_GODS][MAX_GOD_ABILITIES];

static bool _print_final_god_abil_desc(int god, const std::string &final_msg,
                                       const ability_type abil)
{
    // If no message then no power.
    if (final_msg.empty())
        return (false);

    std::ostringstream buf;
    buf << final_msg;
    const std::string cost = "(" + make_cost_description(abil) + ")";
    if (cost != "(None)")
    {
        const int spacesleft = 79 - buf.str().length();
        buf << std::setw(spacesleft) << cost;
    }
    cprintf("%s\n", buf.str().c_str());

    return (true);
}

static bool _print_god_abil_desc(int god, int numpower)
{
    const char* pmsg = god_gain_power_messages[god][numpower];

    // If no message then no power.
    if (!pmsg[0])
        return (false);

    std::string buf;
    if (isupper(pmsg[0]))
        buf = pmsg;             // Complete sentence given.
    else
    {
        buf = "You can ";
        buf += pmsg;
        buf += ".";
    }

    // This might be ABIL_NON_ABILITY for passive abilities.
    const ability_type abil = god_abilities[god][numpower];
    _print_final_god_abil_desc(god, buf, abil);

    return (true);
}

static std::string _describe_favour_generic(god_type which_god)
{
    const std::string godname = god_name(which_god);
    return (you.piety > 130) ? "A prized avatar of " + godname + ".":
           (you.piety > 100) ? "A shining star in the eyes of " + godname + "." :
           (you.piety >  70) ? "A rising star in the eyes of " + godname + "." :
           (you.piety >  40) ? godname + " is most pleased with you." :
           (you.piety >  20) ? godname + " has noted your presence." :
           (you.piety >   5) ? godname + " is noncommittal."
                             : "You are beneath notice.";
}

//---------------------------------------------------------------
//
// describe_god
//
// Describes the player's standing with his deity.
//
//---------------------------------------------------------------

std::string describe_favour(god_type which_god)
{
    if (player_under_penance())
    {
        const int penance = you.penance[which_god];
        return (penance >= 50) ? "Godly wrath is upon you!" :
               (penance >= 20) ? "You've transgressed heavily! Be penitent!" :
               (penance >= 5 ) ? "You are under penance."
                               : "You should show more discipline.";
    }

    return (which_god == GOD_XOM) ? describe_xom_favour()
                                  : _describe_favour_generic(which_god);
}

static std::string _religion_help( god_type god )
{
    std::string result = "";

    switch (god)
    {
    case GOD_ZIN:
        result += "Pray at one of " + god_name(god)
               +  "'s altars to part with your money.";
        break;

    case GOD_SHINING_ONE:
        if (you.haloed())
        {
            int halo_size = halo_radius();
            result += "You radiate a ";

            if (halo_size > 6)
                result += "large ";
            else if (halo_size > 3)
                result += "";
            else
                result += "small ";

            result += "righteous aura, and all beings within it are "
                      "easier to hit.";
        }
        if (!player_under_penance() && you.piety > 160
            && !you.num_gifts[god])
        {
            if (!result.empty())
                result += " ";

            result += "You can pray at an altar to have your weapon "
                      "blessed, especially a long blade.";
        }
        break;

    case GOD_LUGONU:
        if (!player_under_penance() && you.piety > 160
            && !you.num_gifts[god])
        {
            result += "You can pray at an altar to have your weapon "
                      "blessed.";
        }
        break;

    case GOD_NEMELEX_XOBEH:
        result += "You can pray to sacrifice all items on your square. "
                  "Inscribe items with !p, !* or =p to avoid sacrificing "
                  "them accidentally.";
        break;

    case GOD_VEHUMET:
        if (you.piety >= 50)
        {
            result += "Vehumet assists you in casting Conjurations"
                      " and Summonings.";
        }
        break;

    default:
        break;
    }

    if (god_likes_butchery(god))
    {
        if (!result.empty())
            result += " ";

        result += "You can sacrifice corpses by dissecting"
                  " them during prayer.";
    }

    return result;
}

// The various titles granted by the god of your choice.
// Note that Xom doesn't use piety the same way as the other gods, so
// it's completely useless.
const char *divine_title[NUM_GODS][8] =
{
    //  No god
    {"Buglet",             "Firebug",               "Bogeybug",                 "Bugger",
     "Bugbear",            "Bugged One",            "Giant Bug",                "Lord of the Bugs"},

    //  Zin
    {"Sinner",             "Anchorite",             "Apologist",                "Pious",
     "Devout",             "Orthodox",              "Immaculate",               "Bringer of Law"},

    //  the Shining One
    {"Sinner",             "Acolyte",               "Righteous",                "Unflinching",
     "Holy Warrior",       "Exorcist",              "Demon Slayer",             "Bringer of Light"},

    //  Kikubaaqudgha -- scholarly death
    {"Sinner",             "Purveyor of Pain",      "Death's Scholar",          "Merchant of Misery",
     "Death's Artisan",    "Dealer of Despair",     "Black Sun",                "Lord of Darkness"},

    //  Yredelemnul -- zombie death
    {"Sinner",             "Zealot",                "Exhumer",                  "Fey %s",
     "Soul Tainter",       "Sculptor of Flesh",     "Harbinger of Death",       "Grim Reaper"},

    //  Xom
    {"Toy",                "Toy",                   "Toy",                      "Toy",
     "Toy",                "Toy",                   "Toy",                      "Teddy Bear"},

    //  Vehumet -- battle mage theme
    {"Meek",               "Sorcerer's Apprentice", "Scholar of Destruction",   "Caster of Ruination",
     "Battle Magician",    "Warlock",               "Annihilator",              "Luminary of Lethal Lore"},

    //  Okawaru -- battle theme
    {"Coward",             "Struggler",             "Combatant",                "Warrior",
     "Knight",             "Warmonger",             "Commander",                "Victor of a Thousand Battles"},

    //  Makhleb -- chaos theme
    {"Orderly",            "Spawn of Chaos",        "Disciple of Annihilation", "Fanfare of Bloodshed",
     "Fiendish",           "Demolition %s",         "Pandemonic",               "Champion of Chaos"},

    //  Sif Muna -- scholarly theme
    {"Ignorant",           "Disciple",              "Student",                  "Adept",
     "Scribe",             "Scholar",               "Sage",                     "Genius of the Arcane"},

    //  Trog -- anger theme
    {"Faithless",          "Troglodyte",            "Angry Troglodyte",         "Frenzied",
     "%s of Prey",         "Rampant",               "Wild %s",                  "Bane of Scribes"},

    //  Nemelex Xobeh -- alluding to Tarot and cards
    {"Unlucky %s",         "The Pannier",           "The Jester",               "The Fortune-Teller",
     "The Soothsayer",     "The Magus",             "The Cardsharp",            "Hand of Fortune"},

    //  Elyvilon
    {"Sinner",             "Comforter",             "Caregiver",                "Practitioner",
     "Pacifier",           "Purifying %s",          "Faith Healer",             "Bringer of Life"},

    //  Lugonu -- distortion theme
    {"Faithless",          "Abyss-Baptised",        "Unweaver",                 "Distorting %s",
     "Agent of Entropy",   "Schismatic",            "Envoy of Void",            "Corrupter of Planes"},

    //  Beogh -- messiah theme
    {"Apostate",           "Messenger",             "Proselytiser",             "Priest",
     "Missionary",         "Evangelist",            "Apostle",                  "Messiah"}
};

static int _piety_level()
{
    return ((you.piety >  160) ? 7 :
            (you.piety >= 120) ? 6 :
            (you.piety >= 100) ? 5 :
            (you.piety >=  75) ? 4 :
            (you.piety >=  50) ? 3 :
            (you.piety >=  30) ? 2 :
            (you.piety >    5) ? 1
                               : 0 );

}

static void _detailed_god_description(god_type which_god)
{
    clrscr();

    const int width = std::min(80, get_number_of_cols());

    std::string godname = god_name(which_god, true);
    int len = get_number_of_cols() - godname.length();
    textcolor(god_colour(which_god));
    cprintf("%s%s" EOL, std::string(len / 2, ' ').c_str(), godname.c_str());
    textcolor(LIGHTGREY);
    cprintf(EOL);

    std::string broken;
    if (which_god != GOD_NEMELEX_XOBEH)
    {
        broken = get_god_powers(which_god);
        if (!broken.empty())
        {
            linebreak_string2(broken, width);
            formatted_string::parse_block(broken, false).display();
            cprintf(EOL);
            cprintf(EOL);
        }
    }

    if (which_god != GOD_XOM)
    {
        broken = get_god_likes(which_god, true);
        linebreak_string2(broken, width);
        formatted_string::parse_block(broken, false).display();

        broken = get_god_dislikes(which_god, true);
        if (!broken.empty())
        {
            cprintf(EOL);
            cprintf(EOL);
            linebreak_string2(broken, width);
            formatted_string::parse_block(broken, false).display();
        }
        // Some special handling.
        broken = "";
        switch (which_god)
        {
        case GOD_TROG:
            broken = "Note that Trog does not demand training of the "
                     "Invocations skill. All abilities are purely based on "
                     "piety.";
            break;

        case GOD_ZIN:
            broken = "Zin will feed starving followers upon <w>p</w>rayer.";
            break;

        case GOD_ELYVILON:
            broken = "Using your healing abilities on monsters may turn hostile "
                     "ones neutral. Pacification works better on natural beasts "
                     "and worse on demons and undead. If the neutralisation does "
                     "not succeed, then piety, food and Magic are still spent, "
                     "but the monster will not be healed. If you successfully "
                     "pacify a monster, it is healed and you will gain half of "
                     "its experience value and some piety. Pacified monsters try "
                     "to leave the level as quickly as possible.";
            break;

        case GOD_NEMELEX_XOBEH:
            if (which_god == you.religion)
            {
                broken = "The piety increase when sacrificing mostly depends "
                         "on the value of the item. To prevent items from "
                         "being accidentally sacrificed, you can "
                         "<w>i</w>nscribe them with <w>!p</w> (protects the "
                         "whole stack), with <w>=p</w> (protects only the "
                         "item), or with <w>!D</w> (causes item to be ignored "
                         "in sacrifices)."
                         EOL EOL
                         "Nemelex Xobeh gifts various types of decks of cards. "
                         "Each deck type comes in three power levels: plain, "
                         "ornate, legendary. The latter contain very powerful "
                         "card effects, potentially hazardous. High piety and "
                         "Evocations skill help here, as the power of Nemelex' "
                         "abilities is governed by Evocations instead of "
                         "Invocations. The type of the deck gifts strongly "
                         "depends on the dominating item class sacrificed:" EOL
                         "  decks of Escape      -- armour" EOL
                         "  decks of Destruction -- weapons and ammunition" EOL
                         "  decks of Dungeons    -- jewellery, books, "
                                                    "miscellaneous items" EOL
                         "  decks of Summoning   -- corpses, chunks, blood" EOL
                         "  decks of Wonders     -- consumables: food, potions, "
                                                    "scrolls, wands" EOL;
            }
        default:
            break;
        }

        if (!broken.empty())
        {
            cprintf(EOL);
            cprintf(EOL);
            linebreak_string2(broken, width);
            formatted_string::parse_block(broken, false).display();
        }
    }

    const int bottom_line = std::min(30, get_number_of_lines());

    cgotoxy(1, bottom_line);
    formatted_string::parse_string(
        "Press '<w>!</w>' to toggle between the overview and the more detailed "
        "description.").display();

    mouse_control mc(MOUSE_MODE_MORE);

    const int keyin = getch();
    if (keyin == '!')
        describe_god(which_god, true);
}

void describe_god( god_type which_god, bool give_title )
{
    int colour;              // Colour used for some messages.

    clrscr();

    if (give_title)
    {
        textcolor( WHITE );
        cprintf( "                                  Religion" EOL );
        textcolor( LIGHTGREY );
    }

    if (which_god == GOD_NO_GOD) //mv: No god -> say it and go away.
    {
        cprintf( EOL "You are not religious." );
        get_ch();
        return;
    }

    colour = god_colour(which_god);

    // Print long god's name.
    textcolor(colour);
    cprintf( "%s", god_name(which_god, true).c_str());
    cprintf(EOL EOL);

    // Print god's description.
    textcolor(LIGHTGREY);

    std::string god_desc = getLongDescription(god_name(which_god, false));
    const int numcols = get_number_of_cols();
    cprintf("%s", get_linebreak_string(god_desc.c_str(), numcols).c_str());

    // Title only shown for our own god.
    if (you.religion == which_god)
    {
        // Print title based on piety.
        cprintf( EOL "Title - " );
        textcolor(colour);
        std::string title = divine_title[which_god][_piety_level()];
        title = replace_all(title, "%s",
                            species_name(you.species, 1, true, false));

        cprintf("%s", title.c_str());
    }

    // mv: Now let's print favour as Brent suggested.
    // I know these messages aren't perfect so if you can think up
    // something better, do it.

    textcolor(LIGHTGREY);
    cprintf(EOL EOL "Favour - ");
    textcolor(colour);

    //mv: Player is praying at altar without appropriate religion.
    // It means player isn't checking his own religion and so we only
    // display favour and go out.
    if (you.religion != which_god)
    {
        textcolor(colour);
        int which_god_penance = you.penance[which_god];

        // Give more appropriate message for the good gods.
        if (which_god_penance > 0 && is_good_god(which_god))
        {
            if (is_good_god(you.religion))
                which_god_penance = 0;
            else if (!god_hates_your_god(which_god) && which_god_penance >= 5)
                which_god_penance = 2; // == "Come back to the one true church!"
        }

        cprintf( (which_god_penance >= 50)   ? "%s's wrath is upon you!" :
                 (which_god_penance >= 20)   ? "%s is annoyed with you." :
                 (which_god_penance >=  5)   ? "%s well remembers your sins." :
                 (which_god_penance >   0)   ? "%s is ready to forgive your sins." :
                 (you.worshipped[which_god]) ? "%s is ambivalent towards you."
                                             : "%s is neutral towards you.",
                 god_name(which_god).c_str() );
    }
    else
    {
        cprintf(describe_favour(which_god).c_str());

        //mv: The following code shows abilities given by your god (if any).

        textcolor(LIGHTGREY);
        cprintf(EOL EOL "Granted powers:                                                          (Cost)" EOL);
        textcolor(colour);

        // mv: Some gods can protect you from harm.
        // I'm not sure how to explain such divine protection because
        // god isn't really protecting player - he only sometimes saves
        // his life (probably it shouldn't be displayed at all).
        // What about this?
        bool have_any = false;

        if (harm_protection_type hpt =
                god_protects_from_harm(which_god, false))
        {
            have_any = true;

            int prayer_prot = 0;

            if (hpt == HPT_PRAYING || hpt == HPT_PRAYING_PLUS_ANYTIME
                || hpt == HPT_RELIABLE_PRAYING_PLUS_ANYTIME)
            {
                prayer_prot = 100 - 3000/you.piety;
            }

            int prot_chance = 10 + you.piety/10 + prayer_prot; // chance * 100

            const char *how = (prot_chance >= 85) ? "carefully" :
                              (prot_chance >= 55) ? "often" :
                              (prot_chance >= 25) ? "sometimes"
                                                  : "occasionally";
            const char *when =
                (hpt == HPT_PRAYING)              ? " during prayer" :
                (hpt == HPT_PRAYING_PLUS_ANYTIME) ? ", especially during prayer" :
                (hpt == HPT_RELIABLE_PRAYING_PLUS_ANYTIME)
                                                  ? ", and always does so during prayer"
                                                  : "";

            std::string buf = god_name(which_god);
            buf += " ";
            buf += how;
            buf += " watches over you";
            buf += when;
            buf += ".";

            _print_final_god_abil_desc(which_god, buf,
                (hpt == HPT_RELIABLE_PRAYING_PLUS_ANYTIME) ?
                    ABIL_HARM_PROTECTION_II : ABIL_HARM_PROTECTION);
        }

        if (which_god == GOD_ZIN)
        {
            if (zin_sustenance(false))
            {
                have_any = true;
                std::string buf = "Praying to ";
                buf += god_name(which_god);
                buf += " will provide sustenance if starving.";
                _print_final_god_abil_desc(which_god, buf,
                                           ABIL_ZIN_SUSTENANCE);
            }
            const char *how = (you.piety >= 150) ? "carefully" : // res mut. 3
                              (you.piety >= 100) ? "often" :
                              (you.piety >=  50) ? "sometimes" :
                                                   "occasionally";

            cprintf("%s %s shields you from mutagenic effects." EOL,
                    god_name(which_god).c_str(), how);
        }
        else if (which_god == GOD_SHINING_ONE)
        {
            have_any = true;
            const char *how = (you.piety >= 150) ? "carefully" : // l.p. 3
                              (you.piety >= 100) ? "often" :
                              (you.piety >=  50) ? "sometimes" :
                                                   "occasionally";

            cprintf("%s %s shields you from negative energy." EOL,
                    god_name(which_god).c_str(), how);
        }
        else if (which_god == GOD_TROG)
        {
            have_any = true;
            std::string buf = "You can call upon ";
            buf += god_name(which_god);
            buf += " to burn spellbooks in your surroundings.";
            _print_final_god_abil_desc(which_god, buf,
                                       ABIL_TROG_BURN_SPELLBOOKS);
        }
        else if (which_god == GOD_ELYVILON)
        {
            have_any = true;
            std::string buf = "You can call upon ";
            buf += god_name(which_god);
            buf += " to destroy weapons lying on the ground.";
            _print_final_god_abil_desc(which_god, buf,
                                       ABIL_ELYVILON_DESTROY_WEAPONS);
        }
        else if (which_god == GOD_YREDELEMNUL)
        {
            if (yred_injury_mirror(false))
            {
                have_any = true;
                std::string buf = god_name(which_god);
                buf += " mirrors your injuries on your foes during prayer.";
                _print_final_god_abil_desc(which_god, buf,
                                           ABIL_YRED_INJURY_MIRROR);
            }
        }

        // mv: No abilities (except divine protection) under penance
        if (!player_under_penance())
        {
            for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
                if (you.piety >= piety_breakpoint(i)
                    && _print_god_abil_desc(which_god, i))
                {
                    have_any = true;
                }
        }
        if (!have_any)
            cprintf( "None." EOL );
    }

    int bottom_line = get_number_of_lines();
    if (bottom_line > 30)
        bottom_line = 30;

    // Only give this additional information for worshippers.
    if (which_god == you.religion)
    {
        std::string extra = get_linebreak_string(_religion_help(which_god),
                                                 numcols).c_str();
        cgotoxy(1, bottom_line - std::count(extra.begin(), extra.end(), '\n')-1,
                GOTO_CRT);
        textcolor(LIGHTGREY);
        cprintf( "%s", extra.c_str() );
    }

    cgotoxy(1, bottom_line);
    textcolor(LIGHTGREY);
    formatted_string::parse_string(
        "Press '<w>!</w>' to toggle between the overview and the more detailed "
        "description.").display();

    mouse_control mc(MOUSE_MODE_MORE);

    const int keyin = getch();
    if (keyin == '!')
        _detailed_god_description(which_god);
}

std::string get_skill_description(int skill, bool need_title)
{
    std::string lookup = skill_name(skill);
    std::string result = "";

    if (need_title)
    {
        result = lookup;
        result += EOL EOL;
    }

    result += getLongDescription(lookup);

    switch (skill)
    {
    case SK_UNARMED_COMBAT:
    {
        // Give a detailed listing of what attacks the character may perform.
        std::vector<std::string> unarmed_attacks;

        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
            || player_genus(GENPC_DRACONIAN)
            || you.species == SP_MERFOLK && player_is_swimming()
            || player_mutation_level( MUT_STINGER ))
        {
            unarmed_attacks.push_back("slap monsters with your tail");
        }

        if (player_mutation_level(MUT_FANGS)
            || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
        {
            unarmed_attacks.push_back("bite other creatures");
        }
        else if (player_mutation_level(MUT_BEAK))
            unarmed_attacks.push_back("peck at monsters with your beak");

        if (player_mutation_level(MUT_HORNS))
            unarmed_attacks.push_back("do a headbutt attack with your horns");
        else if (you.species == SP_NAGA)
            unarmed_attacks.push_back("do a headbutt attack");

        if (player_mutation_level(MUT_HOOVES))
            unarmed_attacks.push_back("kick your enemies with your hooves");
        else if (player_mutation_level(MUT_TALONS))
            unarmed_attacks.push_back("claw at your enemies with your talons");
        else if (you.species != SP_NAGA
                 && (you.species != SP_MERFOLK || !player_is_swimming()))
        {
            unarmed_attacks.push_back("kick your enemies");
        }

        if (you.equip[EQ_WEAPON] == -1)
            unarmed_attacks.push_back("punch monsters");
        else if (you.equip[EQ_SHIELD] == -1)
            unarmed_attacks.push_back("punch monsters with your free hand");

        if (!unarmed_attacks.empty())
        {
            std::string broken = "For example, you could ";
                        broken += comma_separated_line(unarmed_attacks.begin(),
                                                       unarmed_attacks.end(),
                                                       " or ", ", ");
                        broken += ".";
            linebreak_string2(broken, 72);

            result += EOL;
            result += broken;
        }
        break;
    }
    case SK_INVOCATIONS:
        if (you.species == SP_DEMIGOD)
        {
            result += EOL;
            result += "How on earth did you manage to pick this up?";
        }
        else if (you.religion == GOD_TROG)
        {
            result += EOL;
            result += "Note that Trog doesn't use Invocations, its being too "
                      "closely connected to magic.";
        }
        else if (you.religion == GOD_NEMELEX_XOBEH)
        {
            result += EOL;
            result += "Note that Nemelex uses Evocations rather than "
                      "Invocations.";
        }
        break;

    case SK_EVOCATIONS:
        if (you.religion == GOD_NEMELEX_XOBEH)
        {
            result += EOL;
            result += "This is the skill all of Nemelex' abilities rely on.";
        }
        break;

    case SK_SPELLCASTING:
        if (you.religion == GOD_TROG)
        {
            result += EOL;
            result += "Keep in mind, though, that Trog will greatly disapprove "
                      "of this.";
        }
        break;
    default:
        // No further information.
        break;
    }

    return result;
}

void describe_skill(int skill)
{
    std::ostringstream data;

    data << get_skill_description(skill, true);

    print_description(data.str());
    if (getch() == 0)
        getch();
}

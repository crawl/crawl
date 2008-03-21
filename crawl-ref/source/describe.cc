/*
 *  File:       describe.cc
 *  Summary:    Functions used to print information about various game objects.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <4>      10/14/99     BCR     enummed describe_god()
 *      <3>      10/13/99     BCR     Added GOD_NO_GOD case in describe_god()
 *      <2>      5/20/99      BWR     Replaced is_artifact with
 *                                    is_dumpable_artefact
 *      <1>      4/20/99      JDJ     Reformatted, uses string objects,
 *                                    split out 10 new functions from
 *                                    describe_item(), added
 *                                    get_item_description and
 *                                    is_artifact.
 */

#include "AppHdr.h"
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
#include "menu.h"
#include "message.h"
#include "monstuff.h"
#include "mon-util.h"
#include "newgame.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "stuff.h"
#include "spl-util.h"
#include "tutorial.h"
#include "xom.h"

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

//---------------------------------------------------------------
//
// print_description
//
// Takes a descrip string filled up with stuff from other functions,
// and displays it with minor formatting to avoid cut-offs in mid
// word and such. The character $ is interpreted as a CR.
//
//---------------------------------------------------------------
void print_description( const std::string &d )
{
    std::string::size_type nextLine = std::string::npos;
    unsigned int  currentPos = 0;

    const unsigned int lineWidth = get_number_of_cols() - 1;

    textcolor(LIGHTGREY);

    while(currentPos < d.length())
    {
        if (currentPos != 0)
            cgotoxy(1, wherey() + 1);

        // see if $ sign is within one lineWidth
        nextLine = d.find('$', currentPos);

        if (nextLine >= currentPos && nextLine < currentPos + lineWidth)
        {
            cprintf("%s", (d.substr(currentPos, nextLine-currentPos)).c_str());
            currentPos = nextLine + 1;
            continue;
        }
        
        // Handle real line breaks.  No substitutions necessary, just update
        // the counts.
        nextLine = d.find('\n', currentPos);
        if (nextLine >= currentPos && nextLine < currentPos + lineWidth)
        {
            cprintf("%s", (d.substr(currentPos, nextLine-currentPos)).c_str());
            currentPos = nextLine +1;
            continue;
        }            
        
        // no newline -- see if rest of string will fit.
        if (currentPos + lineWidth >= d.length())
        {
            cprintf((d.substr(currentPos)).c_str());
            return;
        }


        // ok.. try to truncate at space.
        nextLine = d.rfind(' ', currentPos + lineWidth);

        if (nextLine > 0)
        {
            cprintf((d.substr(currentPos, nextLine - currentPos)).c_str());
            currentPos = nextLine + 1;
            continue;
        }

        // oops.  just truncate.
        nextLine = currentPos + lineWidth;

        if (nextLine > d.length())
            nextLine = d.length();

        cprintf((d.substr(currentPos, nextLine - currentPos)).c_str());
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

        // Positive, quantative attributes
        { "AC",     RAP_AC,                    0 },
        { "EV",     RAP_EVASION,               0 },
        { "Str",    RAP_STRENGTH,              0 },
        { "Dex",    RAP_DEXTERITY,             0 },
        { "Int",    RAP_INTELLIGENCE,          0 },
        { "Acc",    RAP_ACCURACY,              0 },
        { "Dam",    RAP_DAMAGE,                0 },

        // Positive, qualitative attributes
        { "MP",     RAP_MAGICAL_POWER,         0 },
        { "SInv",   RAP_EYESIGHT,              2 },
        { "Stlth",  RAP_STEALTH,               2 }  // handled specially
    };

    // For randart jewellery, note the base jewellery type if it's not
    // covered by randart_desc_properties()
    if (item.base_type == OBJ_JEWELLERY
        && item_ident(item, ISFLAG_KNOW_PROPERTIES))
    {
        const std::string type = jewellery_base_ability_string(item.sub_type);
        if ( !type.empty() )
            propnames.push_back(type);
    }
    else if (item.base_type == OBJ_WEAPONS
             && item_ident(item, ISFLAG_KNOW_TYPE))
    {
        std::string ego = weapon_brand_name(item, true);
        if (!ego.empty())
        {
            // ugly hack to remove the brackets...
            ego = ego.substr(2, ego.length()-3);
            
            // and another one for adding a comma if needed
            for ( unsigned i = 0; i < ARRAYSIZE(propanns); ++i )
                 if (known_proprt(propanns[i].prop))
                 {
                     ego += ",";
                     break;
                 }
                 
            propnames.push_back(ego);
        }
    }

    for ( unsigned i = 0; i < ARRAYSIZE(propanns); ++i )
    {
        if (known_proprt(propanns[i].prop))
        {
            const int val = proprt[propanns[i].prop];
            std::ostringstream work;
            switch ( propanns[i].spell_out )
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

static std::string _randart_auto_inscription( const item_def& item )
{
    const std::vector<std::string> propnames = _randart_propnames(item);

    return comma_separated_line(propnames.begin(), propnames.end(),
                                " ", " ");
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

struct property_descriptor
{
    randart_prop_type property;
    const char* desc;           // If it contains %d, will be replaced by value
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
        { RAP_ANGRY, "It makes you angry.", false}
    };

    for ( unsigned i = 0; i < ARRAYSIZE(propdescs); ++i )
    {
        if ( known_proprt(propdescs[i].property))
        {
            std::string sdesc = propdescs[i].desc;

            // FIXME Not the nicest hack.
            char buf[80];
            snprintf(buf, sizeof buf, "%+d", proprt[propdescs[i].property]);
            sdesc = replace_all(sdesc, "%d", buf);

            if ( propdescs[i].is_graded_resist )
            {
                int idx = proprt[propdescs[i].property] + 3;
                if ( idx < 0 )
                    idx = 0;
                if ( idx > 6 )
                    idx = 6;
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
    
    if (s == "random" || s == "any")
        return (TRAP_RANDOM);
    else if (s == "suitable")
        return (TRAP_INDEPTH);
    else if (s == "nonteleport" || s == "noteleport" || s == "nontele"
             || s == "notele")
        return (TRAP_NONTELEPORT);

    for (int i = 0; i < NUM_TRAPS; ++i)
    {
        if (trap_names[i] == s)
            return (i);
    }

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
        if ( !one_chance_in(4) )
            description << RANDOM_ELEMENT(nonfly_names);
        break;
    }

    description << ".";

    if ( random2(40) < 3 )
    {
        if ( player_can_smell() )
        {
            switch ( random2(3) )
            {
            case 0:
                description << " It stinks of brimstone.";
                break;
            case 1:
                description << " It is surrounded by a sickening stench.";
                break;
            case 2:
                description << " It smells like rotting flesh"
                            << (you.mutation[MUT_SAPROVOROUS] == 3 ?
                                " - yum!" : ".");
                break;
            }
        }
    }
    else if (coinflip())
        description << RANDOM_ELEMENT(misc_descs);

    return description.str();
}


//---------------------------------------------------------------
//
// describe_weapon
//
//---------------------------------------------------------------
static std::string _describe_weapon( const item_def &item, bool verbose)
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
            case SPWPN_KNIFE_OF_ACCURACY:
                description += "It is almost unerringly accurate.";
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
                if (is_range_weapon(item))
                {
                    description += "It allows its wielder to fire twice when "
                           "they would otherwise have fired only once.";
                }
                else
                {
                    description += "It allows its wielder to attack twice when "
                           "they would otherwise have struck only once.";
                }
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
            }
        }

        if (is_random_artefact( item ))
        {
            description += "$$";
            description += _randart_descrip( item );

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

    if ( is_demonic(item) && !launcher )
         description += "$Demonspawn are more deadly with it.";
    else if (get_equip_race(item) != ISFLAG_NO_RACE)
    {
        switch (get_equip_race( item ))
        {
        case ISFLAG_DWARVEN:
            description += "$It is well-crafted and very durable.";
            description += "$Dwarves are more deadly with it";
            break;
        case ISFLAG_ELVEN:
            description += "$Elves are more accurate with it";
            break;
        case ISFLAG_ORCISH:
            description += "$Orcs are more deadly with it";
            break;
        }

        if (launcher)
        {
            description += ", and it is most deadly when used with ";
            description += racial_description_string(item);
            description += "ammunition";
        }
        description += ".";
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

    bool need_new_line = true;
    if (item.special && item_type_known(item))
    {
        description += "$$";
        switch (item.special)
        {
        case SPMSL_FLAME:
            description += "When fired from an appropriate launcher, "
                "it turns into a bolt of flame.";
            break;
        case SPMSL_ICE:
            description += "When fired from an appropriate launcher, "
                "it turns into a bolt of ice.";
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
        }
        need_new_line = false;
    }

    if (get_equip_race(item) != ISFLAG_NO_RACE)
    {
        bool can_launch = has_launcher(item);
        bool can_throw = is_throwable(item);

        description += "$";
        if (need_new_line)
            description += "$";

        if (can_throw)
        {
            switch ( get_equip_race(item) )
            {
            case ISFLAG_DWARVEN:
                description +=
                    "It is more deadly when thrown by dwarves";
                break;
            case ISFLAG_ELVEN:
                description +=
                    "It is more deadly when thrown by elves";
                break;
            case ISFLAG_ORCISH:
                description +=
                    "It is more deadly when thrown by orcs";
                break;
            }
        }

        if (can_throw && !can_launch)
            description += ".";
        else if (!can_throw && can_launch)
            description += "It ";
        else if (can_throw && can_launch)
            description += ", and it ";

        if (can_launch)
        {
            description += "is more effective in conjunction with ";
            description += racial_description_string(item);
            description += "launchers.";
        }
    }
    return (description);
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
        description += "$Armour rating: ";
        _append_value(description, property( item, PARM_AC ), false);
        description += "       ";

        description += "Evasion modifier: ";
        _append_value(description, property( item, PARM_EVASION ), true);
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

        // these two are robes only:
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
        }
    }

    if (is_random_artefact( item ))
    {
        description += "$$";
        description += _randart_descrip( item );
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES) && item_type_known(item))
            description += "$This armour may have some hidden properties.";
    }
    else if (get_equip_race( item ) != ISFLAG_NO_RACE)
    {
        description += "$";
        switch (get_equip_race( item ))  
        {
        case ISFLAG_ELVEN:
            description += "$It is well-crafted and unobstructive";

            if (item.sub_type == ARM_CLOAK || item.sub_type == ARM_BOOTS)
                description += ", and helps its wearer avoid being noticed";

            description += ".";
            description += "$It fits elves well.";
            break;

        case ISFLAG_DWARVEN:
            description += "$It is well-crafted and very durable.";
            description += "$It fits dwarves well.";
            break;
        
        case ISFLAG_ORCISH:
            description += "$It fits orcs well.";
            break;

        default:
            break;
        }
    }

    if (verbose && get_armour_slot(item) == EQ_BODY_ARMOUR)
    {
        description += "$$";
        if ( is_light_armour(item) )
            description += "This is a light armour. Wearing it will "
                "exercise Dodging and Stealth.";
        else
            description += "This is a heavy armour. Wearing it will "
                "exercise Armour.";
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
            || (item.sub_type == RING_SLAYING && item.plus2 != 0)
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
                if (item.plus != 0 || is_random_artefact( item ))
                {
                    description += "$It affects your accuracy (";
                    _append_value( description, item.plus, true );
                    description += ").";
                }

                if (item.plus2 != 0 || is_random_artefact( item ))
                {
                    description += "$It affects your damage-dealing abilities (";
                    _append_value( description, item.plus2, true );
                    description += ").";
                }
                break;
            
            default:
                break;
            }
        }
    }

    // randart properties
    if (is_random_artefact( item ))
    {
        description += "$";
        description += _randart_descrip(item);
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES))
        {
            description += "$This ";
            description += (jewellery_is_amulet(item) ? "amulet" : "ring");
            description += " may have hidden properties.";
        }
    }

    return (description);
}                               // end describe_jewellery()

static bool compare_card_names(card_type a, card_type b)
{
    return std::string(card_name(a)) < std::string(card_name(b));
}

//---------------------------------------------------------------
//
// describe_misc_item
//
//---------------------------------------------------------------
static std::string describe_deck( const item_def &item )
{
    std::string description;

    description.reserve(100);

    description += "$";

    const std::vector<card_type> drawn_cards = get_drawn_cards(item);
    if ( !drawn_cards.empty() )
    {
        description += "Drawn card(s): ";
        for ( unsigned int i = 0; i < drawn_cards.size(); ++i )
        {
            if ( i != 0 )
                description += ", ";
            description += card_name(drawn_cards[i]);
        }
        description += "$";
    }

    const int num_cards = cards_in_deck(item);
    if ( top_card_is_known(item) )
    {
        description += "Next card(s): ";
        for ( int i = 0; i < num_cards; ++i )
        {
            unsigned char flags;
            const card_type card = get_card_and_flags(item, -i-1, flags);
            if ( flags & CFLAG_MARKED )
            {
                if ( i != 0 )
                    description += ", ";
                description += card_name(card);
            }
            else
                break;
        }
        description += "$";
    }

    std::vector<card_type> seen_cards;
    for ( int i = 0; i < num_cards; ++i )
    {
        unsigned char flags;
        const card_type card = get_card_and_flags(item, -i-1, flags);
        // This *might* leak a bit of information...oh well.
        if ( (flags & CFLAG_SEEN) && !(flags & CFLAG_MARKED) )
            seen_cards.push_back(card);
    }
    if ( !seen_cards.empty() )
    {
        std::sort(seen_cards.begin(), seen_cards.end(),
                  compare_card_names);
        description += "Seen card(s): ";
        for ( unsigned int i = 0; i < seen_cards.size(); ++i )
        {
            if ( i != 0 )
                description += ", ";
            description += card_name(seen_cards[i]);
        }
        description += "$";
    }

    return (description);
}

// ========================================================================
//      Public Functions
// ========================================================================

bool is_dumpable_artefact( const item_def &item, bool verbose)
{
    bool ret = false;

    if (is_artefact( item ) )
    {
        ret = item_ident( item, ISFLAG_KNOW_PROPERTIES );
    }
    else if (item.base_type == OBJ_ARMOUR
        && (verbose && item_type_known(item)))
    {
        const int spec_ench = get_armour_ego_type( item );
        ret = (spec_ench >= SPARM_RUNNING && spec_ench <= SPARM_PRESERVATION);
    }
    else if (item.base_type == OBJ_JEWELLERY 
        && (verbose && item_type_known(item)))
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
                                  bool dump )
{
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
                    << "x: " << item.x << " y: " << item.y
                    << " link: " << item.link
                    << " ident_type: "
                    << static_cast<int>(get_ident_type(item.base_type,
                                                       item.sub_type));
    }
#endif

    if (is_unrandom_artefact( item )
        && strlen(unrandart_descrip(1, item)) != 0)
    {
        description << "$$";
        description << unrandart_descrip(1, item);
        description << "$";
    }
    else if (is_fixed_artefact(item) && item_type_known(item))
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
                         && item.base_type != OBJ_ARMOUR))
    {
        description << "$$";
        std::string db_name = item.name(DESC_DBNAME, true, false, false);
        std::string db_desc = getLongDescription(db_name);

        if (db_desc == "")
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

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        description << _describe_weapon( item, verbose );
        break;

    case OBJ_MISSILES:
        description << _describe_ammo( item );
        break;

    case OBJ_ARMOUR:
        description << _describe_armour( item, verbose );
        break;

    case OBJ_WANDS:
        if (item_ident( item, ISFLAG_KNOW_PLUSES ) && item.plus == 0)
            description << "Unfortunately, it has no charges left.";
        description << "$";
        break;

    case OBJ_FOOD:
        if (item.sub_type == FOOD_CHUNK)
        {
            if (food_is_rotten(item))
            {
                if (you.mutation[MUT_SAPROVOROUS] == 3)
                    description << "It looks nice and ripe.";
                else if (you.is_undead != US_UNDEAD)
                {
                    description << "In fact, it is "
                        "rotting away before your eyes. "
                        "Eating it would probably be unwise.";
                }
            }
            else if (you.mutation[MUT_SAPROVOROUS] < 3)
                description << "It looks rather unpleasant.";

            description << "$";
        }
        break;

    case OBJ_JEWELLERY:
        description << _describe_jewellery( item, verbose );
        break;

    case OBJ_STAVES:
        if (item_type_known(item))
        {
            if (item_is_rod( item ))
            {
                description <<
                    "$It uses its own mana reservoir for casting spells, and "
                    "recharges automatically by channeling mana from its "
                    "wielder.";
            }
            else
            {
                description <<
                    "$Damage rating: 9    Accuracy rating: +2    "
                    "Attack delay: 120%";

                description << "$$It falls into the 'staves' category.";
            }
        }
        break;

    case OBJ_MISCELLANY:
        if (is_deck(item))
            description << describe_deck( item );
        break;

    case OBJ_BOOKS:
        if (! player_can_read_spellbook( item ))
            description << "$This book is beyond your current level of understanding.";
        break;

    case OBJ_SCROLLS:
    case OBJ_POTIONS:
    case OBJ_ORBS:
    case OBJ_CORPSES:
    case OBJ_GOLD:
        // No extra processing needed for these item types.
        break;

    default:
        DEBUGSTR("Bad item class");
        description << "$This item should not exist. Mayday! Mayday!";
    }

    if (!verbose && item_known_cursed( item ))
        description << "It has a curse placed upon it.";
    else
    {
        description << "$$It";
        if (item_known_cursed( item ))
            description << " has a curse placed upon it, and it";

        const int mass = item_mass( item );
        description << " weighs around " << (mass / 10)
                    << "." << (mass % 10)
                    << " aum. "; // arbitrary unit of mass

        if ( is_dumpable_artefact(item, false) )
        {
            if (item.base_type == OBJ_ARMOUR || item.base_type == OBJ_WEAPONS)
                description << "$$This ancient artefact cannot be changed "
                    "by magic or mundane means.";
            else
                description << "$$It is an ancient artefact.";
        }
    }

    return description.str();
}                               // end get_item_description()

static std::string get_feature_description_wide(int feat)
{
    return std::string();
}

void describe_feature_wide(int x, int y)
{
    std::string desc = feature_description(x, y);
    desc += "$$";

    // Get rid of trailing .$$ before lookup
    desc +=
        getLongDescription(grd[x][y] == DNGN_ENTER_SHOP? "A shop"
                           : desc.substr(0, desc.length() - 3));

    // For things which require logic
    desc += get_feature_description_wide(grd[x][y]);

    clrscr();
    print_description(desc);

#ifdef USE_TILE
    mouse_control mc(MOUSE_MODE_MORE);
#endif

    if (Options.tutorial_left)
        tutorial_describe_feature(grd[x][y]);
        
    if ( getch() == 0 )
        getch();
}

// Return true if spells can be shown to player
static bool show_item_description(const item_def &item)
{
    clrscr();

    const std::string description = get_item_description( item, 1 );
    print_description(description);
    if (Options.tutorial_left)
        tutorial_describe_item(item);

    if (item.has_spells())
    {
        if (item.base_type == OBJ_BOOKS && !player_can_read_spellbook( item ))
            return false;
        formatted_string fs;
        item_def dup = item;
        spellbook_contents( dup, 
                item.base_type == OBJ_BOOKS? 
                    RBOOK_READ_SPELL
                  : RBOOK_USE_STAFF, 
                &fs );
        fs.display(2, -2);
        return true;
    }

    return false;
}

static bool describe_spells(const item_def &item)
{
    int c = getch();
    if (c < 'a' || c > 'h')     //jmf: was 'g', but 8=h
    {
        mesclr( true );
        return (false);
    }

    const int spell_index = letter_to_index(c);

    spell_type nthing =
        which_spell_in_book(item.book_number(), spell_index);
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
    for (;;)
    {
        const bool spells_shown = show_item_description(item);

        if (spells_shown)
        {
            cgotoxy(1, wherey());
            textcolor(LIGHTGREY);
            cprintf("Select a spell to read its description.");
            if (describe_spells(item))
                continue;
            return;
        }
        break;
    }
    
    // Don't ask during tutorial, or if there aren't enough rows left
    if (!Options.tutorial_left && allow_inscribe
        && wherey() <= get_number_of_lines() - 3)
    {
        cgotoxy(1, wherey() + 2);

        std::string ainscrip;

        if (is_random_artefact(item))
            ainscrip = _randart_auto_inscription(item);

        // Only allow autoinscription if we don't have all the text
        // already.
        const bool allow_autoinscribe =
            is_random_artefact(item)
            && !ainscrip.empty()
            && item.inscription.find(ainscrip) == std::string::npos;

        if ( allow_autoinscribe )
        {
            formatted_string::parse_string(
                "<cyan>Do you wish to inscribe this item? "
                "('a' to autoinscribe) ").display();
        }
        else
        {
            formatted_string::parse_string(
                "<cyan>Do you wish to inscribe this item? ").display();
        }

        const int keyin = getch();

        if (toupper(keyin) == 'Y')
        {
            char buf[79];
            cprintf("\nInscribe with what? ");
            if (!cancelable_get_line(buf, sizeof buf))
            {
                item.inscription = buf;
                you.quiver_change = true;       // might have added/removed !F 
            }
        }
        else if (toupper(keyin) == 'A' && allow_autoinscribe)
        {
            // Remove previous randart inscription
            _trim_randart_inscrip(item);

            if (!item.inscription.empty())
                item.inscription += ", ";

            item.inscription += ainscrip;
        }
    }
    else if (getch() == 0)
        getch();
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
    if ( !long_descrip.empty() )
        description += long_descrip;
    else
    {
        description += "This spell has no description. "
            "Casting it may therefore be unwise. "
#if DEBUG
            "Instead, go fix it. ";
#else
        "Please file a bug report.";
#endif // DEBUG
    }

    clrscr();
    print_description(description);

#ifdef USE_TILE
    mouse_control mc(MOUSE_MODE_MORE);
#endif

    if (getch() == 0)
        getch();
}                               // end describe_spell()

static std::string describe_draconian_role(const monsters *mon)
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

static std::string describe_draconian_colour(int species)
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

static std::string describe_draconian(const monsters *mon)
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
        if (describe_draconian_colour(subsp) != "")
            description += "  " + describe_draconian_colour(subsp);
    }

    if (subsp != mon->type)
    {
        if (describe_draconian_role(mon) != "")
            description += "  " + describe_draconian_role(mon);
    }

    return (description);
}

//---------------------------------------------------------------
//
// describe_monsters
//
// Contains sketchy descriptions of every monster in the game.
//
//---------------------------------------------------------------
void describe_monsters(monsters& mons)
{
    // for undetected mimics describe mimicked item instead
    if (mons_is_mimic(mons.type) && !(mons.flags & MF_KNOWN_MIMIC))
    {
        item_def item;
        const monsters *mon = &mons;
        get_mimic_item( mon, item );
        describe_item(item);
        return;
    }
    
    std::ostringstream description;
    description << mons.name(DESC_CAP_A) << "$$";
    
    // Note: Nearly all of the "long" descriptions have moved to 
    // mon-data.h, in an effort to give them some locality with the
    // rest of the monster definition data, and to get them out of
    // the control logic.  The only sorts of descriptions that should
    // go here are those that require active decisions about what
    // sort of message to print (eg "It's beautiful" or "It's ugly" 
    // depending on player class).  And just between you and me: that's
    // sort of a dumb idea anyway.  So don't add any more of those.
    //
    // In my fantasy world, the long descriptions (and monster data)
    // wouldn't live in a header file, but in a simple text file
    // that's easier to edit and easy to read.  Even XML would be better
    // than what we have today.
    //
    // -peterb 4/14/07

    if (mons_is_mimic(mons.type) && mons.type != MONS_GOLD_MIMIC)
        description << getLongDescription("mimic");
    else
        description << getLongDescription(mons.name(DESC_PLAIN));

    std::string symbol = "";
    symbol += get_monster_data(mons.type)->showchar;
    if (isupper(symbol[0]))
        symbol = "cap-" + symbol;

    std::string symbol_prefix = "__";
    symbol_prefix += symbol;
    symbol_prefix += "_prefix";
    description << getLongDescription(symbol_prefix);

    switch (mons.type)
    {
    case MONS_ZOMBIE_SMALL: case MONS_ZOMBIE_LARGE:
        description << getLongDescription("zombie");
        break;

    case MONS_SKELETON_SMALL: case MONS_SKELETON_LARGE:
        description << getLongDescription("skeleton");
        break;

    case MONS_SIMULACRUM_SMALL: case MONS_SIMULACRUM_LARGE:
        description << getLongDescription("large simulacrum");
        break;

    case MONS_SPECTRAL_THING:
        description << getLongDescription("spectral thing");
        break;

    case MONS_ROTTING_DEVIL:
        if (player_can_smell())
        {
            if (you.mutation[MUT_SAPROVOROUS] == 3)
                description << "It smells great!";
            else
                description << "It stinks.";
        }
        break;

    case MONS_SWAMP_DRAKE:
        if (player_can_smell())
            description << "It smells horrible.";
        break;

    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GUARDIAN_NAGA:
    case MONS_GREATER_NAGA:
        if (you.species == SP_NAGA)
            description << "It is particularly attractive.";
        else
            description << "It is strange and repulsive.";
        break;

    case MONS_VAMPIRE:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAMPIRE_MAGE:
        if (you.is_undead == US_ALIVE)
            description << " It wants to drink your blood! ";
        break;

    case MONS_REAPER:
        if (you.is_undead == US_ALIVE)
            description << "It has come for your soul!";
        break;

    case MONS_ELF:
        // These are only possible from polymorphing or shapeshifting.
        description << "This one is remarkably plain looking.";
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
        description << describe_draconian( &mons );
        break;        
    }
    case MONS_PLAYER_GHOST:
        description << "The apparition of " << ghost_description(mons) << ".$";
        break;

    case MONS_PANDEMONIUM_DEMON:
        description << _describe_demon(mons);
        break;

    case MONS_URUG:
        if (player_can_smell())
            description << "He smells terrible.";
        break;

    case MONS_PROGRAM_BUG:
        description << "If this monster is a \"program bug\", then it's "
            "recommended that you save your game and reload.  Please report "
            "monsters who masquerade as program bugs or run around the "
            "dungeon without a proper description to the authorities.";
        break;
    default:
        break;
    }

    description << "\n";
    std::string symbol_suffix = "__";
    symbol_suffix += symbol;
    symbol_suffix += "_suffix";
    description << getLongDescription(symbol_suffix);

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
                    description << "$$Monster Spells:$";
                    found_spell = true;
                }

                description << "    " << i << ": "
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
                description << "$Monster Inventory:$";
                has_item = true;
            }
            description << "    " << i << ") "
                        << mitm[mons.inv[i]].name(DESC_NOCAP_A, false, true);
        }
    }
#endif

    clrscr();
    print_description(description.str());

#ifdef USE_TILE
    mouse_control mc(MOUSE_MODE_MORE);
#endif

    if (Options.tutorial_left)
        tutorial_describe_monster(static_cast<const monsters*>(&mons));

    if (getch() == 0)
        getch();
}                               // end describe_monsters

//---------------------------------------------------------------
//
// ghost_description
//
// Describes the current ghost's previous owner. The caller must
// prepend "The apparition of" or whatever and append any trailing
// punctuation that's wanted.
//
//---------------------------------------------------------------
std::string ghost_description(const monsters &mons, bool concise)
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
    case SP_OGRE_MAGE:
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
    if ( concise )
        gstr << get_species_abbrev(gspecies)
             << get_class_abbrev(ghost.job);
    else
        gstr << species_name(gspecies,
                             ghost.xl)
             << " "
             << get_class_name(ghost.job);

    return gstr.str();
}

extern ability_type god_abilities[MAX_NUM_GODS][MAX_GOD_ABILITIES];

static bool print_god_abil_desc( int god, int numpower )
{
    const char* pmsg = god_gain_power_messages[god][numpower];

    // if no message then no power
    if ( !pmsg[0] )
        return false;
    
    std::ostringstream buf;

    if ( isupper(pmsg[0]) )
        buf << pmsg;            // complete sentence given
    else
        buf << "You can " << pmsg << ".";

    // this might be ABIL_NON_ABILITY for passive abilities
    const ability_type abil = god_abilities[god][numpower];

    if ( abil != ABIL_NON_ABILITY )
    {
        const int spacesleft = 79 - buf.str().length();
        const std::string cost = "(" + make_cost_description(abil) + ")";
        buf << std::setw(spacesleft) << cost;
    }
    
    cprintf( "%s\n", buf.str().c_str() );
    return true;
}

static std::string describe_favour_generic(god_type which_god)
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

    return (which_god == GOD_XOM)?
            describe_xom_favour() : describe_favour_generic(which_god);
}

static std::string religion_help( god_type god )
{
    std::string result = "";
    
    switch(god)
    {
    case GOD_ZIN:
        result += "Pray at one of " + god_name(god)
               +  "'s altars to part with your money.";
        break;
        
    case GOD_SHINING_ONE:
        if (!player_under_penance() && you.piety > 160
            && !you.num_gifts[god])
        {
            result += "You can pray at an altar to ask " + god_name(god)
                   +  " to bless a long sword.";
        }
        break;

    case GOD_LUGONU:
        if (!player_under_penance() && you.piety > 160
            && !you.num_gifts[god])
        {
            result += "You can pray at an altar to ask " + god_name(god)
                   +  " to bless your weapon." EOL;
        } // fall through
           
    case GOD_OKAWARU:
    case GOD_MAKHLEB:
    case GOD_BEOGH:
    case GOD_TROG:
        result = "You can sacrifice corpses by dissecting "
                 "them during prayer.";
        break;

    case GOD_NEMELEX_XOBEH:
        result += "You can pray to sacrifice all items on your square. "
                  "Inscribe items with !p, !* or =p to avoid sacrificing "
                  "them accidentally.";
        break;

    case GOD_VEHUMET:
        if (you.piety >= 50)
            result += "Vehumet assists you in casting Conjurations"
                      " and Summonings during prayer.";
        break;

    default:
        break;
    }
    return result;
}

void describe_god( god_type which_god, bool give_title )
{
    int         colour;      // mv: colour used for some messages

    clrscr();

    if (give_title)
    {
        textcolor( WHITE );
        cprintf( "                                  Religion" EOL );
        textcolor( LIGHTGREY );
    }

    if (which_god == GOD_NO_GOD) //mv:no god -> say it and go away
    {
        cprintf( EOL "You are not religious." );
        get_ch();
        return;
    }

    colour = god_colour(which_god);

    //mv: print god's name and title - if you can think up better titles
    //I have nothing against
    textcolor(colour);
    cprintf( "%s", god_name(which_god, true).c_str()); //print long god's name
    cprintf (EOL EOL);

    //mv: print god's description
    textcolor(LIGHTGRAY);

    std::string god_desc = getLongDescription(god_name(which_god, false));
    const int numcols = get_number_of_cols();
    cprintf("%s", get_linebreak_string(god_desc.c_str(), numcols).c_str());

    // title only shown for our own god
    if (you.religion == which_god)
    {
        //mv: print title based on piety
        cprintf( EOL EOL "Title - " );
        textcolor(colour);

        // mv: if your piety is high enough you get title
        // based on your god
        if (you.piety > 160)
        {
            cprintf("%s",
                (which_god == GOD_ZIN)           ? "Champion of Law" :
                (which_god == GOD_SHINING_ONE)   ? "Divine Warrior" :
                (which_god == GOD_ELYVILON)      ? "Champion of Light" :
                (which_god == GOD_OKAWARU)       ? "Master of a Thousand Battles" :
                (which_god == GOD_YREDELEMNUL)   ? "Master of Eternal Death" :
                (which_god == GOD_KIKUBAAQUDGHA) ? "Lord of Darkness" :
                (which_god == GOD_MAKHLEB)       ? "Champion of Chaos" :
                (which_god == GOD_VEHUMET)       ? "Lord of Destruction" :
                (which_god == GOD_TROG)          ? "Great Slayer" :
                (which_god == GOD_NEMELEX_XOBEH) ? "Great Trickster" :
                (which_god == GOD_SIF_MUNA)      ? "Master of the Arcane" :
                (which_god == GOD_LUGONU)        ? "Agent of Entropy" :
                (which_god == GOD_BEOGH)         ? "Messiah" :
                (which_god == GOD_XOM)           ? "Teddy Bear"
                                                 : "Bogy the Lord of the Bugs");
        }
        else
        {
            //mv: most titles are still universal - if any one wants to
            //he might write specific titles for all gods or rewrite current
            //ones (I know they are not perfect)
            //btw. titles are divided according to piety levels on which you get
            //new abilities.In the main it means - new ability = new title
            switch (which_god)
            {
            case GOD_ZIN:
            case GOD_SHINING_ONE:
            case GOD_KIKUBAAQUDGHA:
            case GOD_YREDELEMNUL:
            case GOD_VEHUMET:
            case GOD_OKAWARU:
            case GOD_MAKHLEB:
            case GOD_TROG:
            case GOD_NEMELEX_XOBEH:
            case GOD_ELYVILON:
            case GOD_LUGONU:
                cprintf ( (you.piety >= 120) ? "High Priest" :
                          (you.piety >= 100) ? "Elder" :
                          (you.piety >=  75) ? "Priest" :
                          (you.piety >=  50) ? "Deacon" :
                          (you.piety >=  30) ? "Novice" :
                          (you.piety >    5) ? "Believer"
                                             : "Sinner" );
                break;
            case GOD_SIF_MUNA:
                cprintf ( (you.piety >= 120) ? "Oracle" :
                          (you.piety >= 100) ? "Scholar" :
                          (you.piety >=  75) ? "Adept" :
                          (you.piety >=  50) ? "Disciple" :
                          (you.piety >=  30) ? "Apprentice" :
                          (you.piety >    5) ? "Believer"
                                             : "Sinner" );
                break;
            case GOD_BEOGH:
                cprintf ( (you.piety >= 120) ? "Saint" :
                          (you.piety >= 100) ? "High Priest" :
                          (you.piety >=  75) ? "Missionary" :
                          (you.piety >=  50) ? "Priest" :
                          (you.piety >=  30) ? "Disciple" :
                          (you.piety >    5) ? "Believer"
                                             : "Sinner" );
                 break;
            case GOD_XOM:
                cprintf("Toy");
            break;

            default: 
                cprintf ("Bug");
            }
        }
    }
    // end of print title

    // mv: now let's print favor as Brent suggested
    // I know these messages aren't perfect so if you can
    // think up something better, do it

    textcolor(LIGHTGRAY);
    cprintf(EOL EOL "Favour - ");
    textcolor(colour);

    //mv: player is praying at altar without appropriate religion
    //it means player isn't checking his own religion and so we only
    //display favour and will go out
    if (you.religion != which_god)
    {
        textcolor (colour);
        cprintf( (you.penance[which_god] >= 50) ? "%s's wrath is upon you!" :
                 (you.penance[which_god] >= 20) ? "%s is annoyed with you." :
                 (you.penance[which_god] >=  5) ? "%s well remembers your sins." :
                 (you.penance[which_god] >   0) ? "%s is ready to forgive your sins." :
                 (you.worshipped[which_god])    ? "%s is ambivalent towards you." 
                                                : "%s is neutral towards you.",
                 god_name(which_god).c_str() );
    } 
    else
    {
        cprintf(describe_favour(which_god).c_str());

        //mv: following code shows abilities given from god (if any)
        textcolor(LIGHTGRAY);
        cprintf(EOL EOL "Granted powers:                                                          (Cost)" EOL);
        textcolor(colour);

        // mv: some gods can protect you from harm
        // I'm not sure how to explain such divine protection because
        // god isn't really protecting player - he only sometimes saves
        // his life (probably it shouldn't be displayed at all).
        // What about this?
        bool have_any = false;

        if (harm_protection_type hpt =
                god_protects_from_harm(which_god, false))
        {
            int prayer_prot = 0;
            if (you.religion == GOD_ELYVILON && you.piety > 30)
                prayer_prot = 100 - 3000/you.piety;

            int prot_chance = 10 + you.piety/10 + prayer_prot; // chance * 100
    
            const char *how = (prot_chance >= 85) ? "carefully" :
                              (prot_chance >= 55) ? "often" :
                              (prot_chance >= 25) ? "sometimes" :
                                                    "occasionally";
            const char *when =
                (hpt == HPT_PRAYING)              ? " during prayer" :
                (hpt == HPT_PRAYING_PLUS_ANYTIME) ? ", especially during prayer" :
                                                    "";

            have_any = true;
            cprintf( "%s %s watches over you%s." EOL,
                     god_name(which_god).c_str(), how, when );
        }

        if (which_god == GOD_ZIN)
        {
            have_any = true;
            if (you.piety >= 30)
                cprintf("Praying to %s will provide sustenance if starving."
                            EOL, god_name(which_god).c_str());

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
            cprintf("You can call upon %s to burn books in your surroundings."
                    EOL, god_name(which_god).c_str());
        }
        else if (which_god == GOD_ELYVILON)
        {
            have_any = true;
            cprintf("You can call upon %s to destroy weapons "
                    "lying on the ground." EOL, god_name(which_god).c_str());
        }

        // mv: No abilities (except divine protection) under penance
        if (!player_under_penance())
        {
            for ( int i = 0; i < MAX_GOD_ABILITIES; ++i )
                if ( you.piety >= piety_breakpoint(i) )
                    if (print_god_abil_desc(which_god, i))
                        have_any = true;
        }
        if ( !have_any )
            cprintf( "None." EOL );
    }

    // only give this additional information for worshippers
    if ( which_god == you.religion )
    {
        if (you.religion == GOD_ZIN || you.religion == GOD_SHINING_ONE)
            cgotoxy(1, get_number_of_lines(), GOTO_CRT);
        else if (you.religion == GOD_BEOGH)
            cgotoxy(1, get_number_of_lines() - 1, GOTO_CRT);
        else
            cgotoxy(1, get_number_of_lines() - 2, GOTO_CRT);

        textcolor(LIGHTGRAY);
        cprintf(get_linebreak_string(religion_help(which_god),
                                     numcols).c_str());
    }

#ifdef USE_TILE
    mouse_control mc(MOUSE_MODE_MORE);
#endif

    get_ch();
}

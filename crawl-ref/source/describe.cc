/**
 * @file
 * @brief Functions used to print information about various game objects.
**/

#include "AppHdr.h"

#include "describe.h"

#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <stdio.h>

#include "artefact.h"
#include "branch.h"
#include "clua.h"
#include "command.h"
#include "database.h"
#include "decks.h"
#include "delay.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "fight.h"
#include "food.h"
#include "ghost.h"
#include "goditem.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-book.h"
#include "mon-chimera.h"
#include "mon-tentacle.h"
#include "options.h"
#include "output.h"
#include "process_desc.h"
#include "prompt.h"
#include "religion.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "spl-wpnench.h"
#include "stash.h"
#include "state.h"
#include "terrain.h"
#ifdef USE_TILE_LOCAL
 #include "tilereg-crt.h"
#endif
#include "unicode.h"

// ========================================================================
//      Internal Functions
// ========================================================================

//---------------------------------------------------------------
//
// append_value
//
// Appends a value to the string. If plussed == 1, will add a + to
// positive values.
//
//---------------------------------------------------------------
static void _append_value(string & description, int valu, bool plussed)
{
    char value_str[80];
    sprintf(value_str, plussed ? "%+d" : "%d", valu);
    description += value_str;
}

static void _append_value(string & description, float fvalu, bool plussed)
{
    char value_str[80];
    sprintf(value_str, plussed ? "%+.1f" : "%.1f", fvalu);
    description += value_str;
}

int count_desc_lines(const string &_desc, const int width)
{
    string desc = get_linebreak_string(_desc, width);

    int count = 0;
    for (int i = 0, size = desc.size(); i < size; ++i)
    {
        const char ch = desc[i];

        if (ch == '\n')
            count++;
    }

    return count;
}

static void _adjust_item(item_def &item);

//---------------------------------------------------------------
//
// print_description
//
// Takes a descrip string filled up with stuff from other functions,
// and displays it with minor formatting to avoid cut-offs in mid
// word and such.
//
//---------------------------------------------------------------

void print_description(const string &body)
{
    describe_info inf;
    inf.body << body;
    print_description(inf);
}

class default_desc_proc
{
public:
    int width() { return get_number_of_cols() - 1; }
    int height() { return get_number_of_lines(); }
    void print(const string &str) { cprintf("%s", str.c_str()); }

    void nextline()
    {
        if (wherey() < height())
            cgotoxy(1, wherey() + 1);
        else
            cgotoxy(1, height());
        // Otherwise cgotoxy asserts; let's just clobber the last line
        // instead, which should be noticeable enough.
    }
};

void print_description(const describe_info &inf)
{
    clrscr();
    textcolour(LIGHTGREY);

    default_desc_proc proc;
    process_description<default_desc_proc>(proc, inf);
}

static void _print_quote(const describe_info &inf)
{
    clrscr();
    textcolour(LIGHTGREY);

    default_desc_proc proc;
    process_quote<default_desc_proc>(proc, inf);
}

static const char* _jewellery_base_ability_string(int subtype)
{
    switch (subtype)
    {
    case RING_SUSTAIN_ABILITIES: return "SustAb";
    case RING_WIZARDRY:          return "Wiz";
    case RING_FIRE:              return "Fire";
    case RING_ICE:               return "Ice";
    case RING_TELEPORTATION:     return "+/*Tele";
    case RING_TELEPORT_CONTROL:  return "+cTele";
    case AMU_CLARITY:            return "Clar";
    case AMU_WARDING:            return "Ward";
    case AMU_RESIST_CORROSION:   return "rCorr";
    case AMU_THE_GOURMAND:       return "Gourm";
#if TAG_MAJOR_VERSION == 34
    case AMU_CONSERVATION:       return "Cons";
    case AMU_CONTROLLED_FLIGHT:  return "cFly";
#endif
    case AMU_RESIST_MUTATION:    return "rMut";
    case AMU_GUARDIAN_SPIRIT:    return "Spirit";
    case AMU_FAITH:              return "Faith";
    case AMU_STASIS:             return "Stasis";
    case AMU_INACCURACY:         return "Inacc";
    }
    return "";
}

#define known_proprt(prop) (proprt[(prop)] && known[(prop)])

struct property_annotators
{
    const char* name;
    artefact_prop_type prop;
    int spell_out;              // 0: "+3", 1: "+++", 2: value doesn't matter
};

static vector<string> _randart_propnames(const item_def& item,
                                         bool no_comma = false)
{
    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_desc_properties(item, proprt, known, true);

    vector<string> propnames;

    // list the following in rough order of importance
    const property_annotators propanns[] =
    {
        // (Generally) negative attributes
        // These come first, so they don't get chopped off!
        { "-Cast",  ARTP_PREVENT_SPELLCASTING,  2 },
        { "-Tele",  ARTP_PREVENT_TELEPORTATION, 2 },
        { "Contam", ARTP_MUTAGENIC,             2 },
        { "*Rage",  ARTP_ANGRY,                 2 },
        { "*Tele",  ARTP_CAUSE_TELEPORTATION,   2 },
        { "Noisy",  ARTP_NOISES,                2 },

        // Evokable abilities come second
        { "+Blink", ARTP_BLINK,                 2 },
        { "+Rage",  ARTP_BERSERK,               2 },
        { "+Inv",   ARTP_INVISIBLE,             2 },
        { "+Fly",   ARTP_FLY,                   2 },
        { "+Fog",   ARTP_FOG,                   2 },

        // Resists, also really important
        { "rElec",  ARTP_ELECTRICITY,           2 },
        { "rPois",  ARTP_POISON,                2 },
        { "rF",     ARTP_FIRE,                  1 },
        { "rC",     ARTP_COLD,                  1 },
        { "rN",     ARTP_NEGATIVE_ENERGY,       1 },
        { "MR",     ARTP_MAGIC,                 1 },
        { "rMut",   ARTP_RMUT,                  2 },
        { "rCorr",  ARTP_RCORR,                 2 },

        // Quantitative attributes
        { "HP",     ARTP_HP,                    0 },
        { "MP",     ARTP_MAGICAL_POWER,         0 },
        { "AC",     ARTP_AC,                    0 },
        { "EV",     ARTP_EVASION,               0 },
        { "Str",    ARTP_STRENGTH,              0 },
        { "Dex",    ARTP_DEXTERITY,             0 },
        { "Int",    ARTP_INTELLIGENCE,          0 },
        { "Slay",   ARTP_SLAYING,               0 },

        // Qualitative attributes (and Stealth)
        { "SInv",   ARTP_EYESIGHT,              2 },
        { "Stlth",  ARTP_STEALTH,               1 },
        { "Curse",  ARTP_CURSED,                2 },
        { "Clar",   ARTP_CLARITY,               2 },
        { "RMsl",   ARTP_RMSL,                  2 },
        { "Regen",  ARTP_REGENERATION,          2 },
        { "SustAb", ARTP_SUSTAB,                2 },
    };

    // For randart jewellery, note the base jewellery type if it's not
    // covered by artefact_desc_properties()
    if (item.base_type == OBJ_JEWELLERY
        && (item_ident(item, ISFLAG_KNOW_TYPE)))
    {
        const char* type = _jewellery_base_ability_string(item.sub_type);
        if (*type)
            propnames.push_back(type);
    }
    else if (item_ident(item, ISFLAG_KNOW_TYPE)
             || is_artefact(item)
                && artefact_known_wpn_property(item, ARTP_BRAND))
    {
        string ego;
        if (item.base_type == OBJ_WEAPONS)
            ego = weapon_brand_name(item, true);
        else if (item.base_type == OBJ_ARMOUR)
            ego = armour_ego_name(item, true);
        if (!ego.empty())
        {
            // XXX: Ugly hack for adding a comma if needed.
            for (unsigned i = 0; i < ARRAYSZ(propanns); ++i)
                if (known_proprt(propanns[i].prop)
                    && propanns[i].prop != ARTP_BRAND
                    && !no_comma)
                {
                    ego += ",";
                    break;
                }

            propnames.push_back(ego);
        }
    }

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.special);
        if (entry && entry->inscrip != NULL)
            propnames.push_back(entry->inscrip);
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
                    && (propanns[i].prop == ARTP_FIRE && val == 1
                        || propanns[i].prop == ARTP_COLD && val == -1))
                {
                    continue;
                }
                if (item.sub_type == RING_ICE
                    && (propanns[i].prop == ARTP_COLD && val == 1
                        || propanns[i].prop == ARTP_FIRE && val == -1))
                {
                    continue;
                }
            }

            ostringstream work;
            switch (propanns[i].spell_out)
            {
            case 0: // e.g. AC+4
                work << showpos << propanns[i].name << val;
                break;
            case 1: // e.g. F++
            {
                // XXX: actually handle absurd values instead of displaying
                // the wrong number of +s or -s
                const int sval = min(abs(val), 6);
                work << propanns[i].name
                     << string(sval, (val > 0 ? '+' : '-'));
                break;
            }
            case 2: // e.g. rPois or SInv
                if (propanns[i].prop == ARTP_CURSED && val < 1)
                    continue;

                work << propanns[i].name;
                break;
            }
            propnames.push_back(work.str());
        }
    }

    return propnames;
}

string artefact_inscription(const item_def& item)
{
    if (item.base_type == OBJ_BOOKS)
        return "";

    const vector<string> propnames = _randart_propnames(item);

    string insc = comma_separated_line(propnames.begin(), propnames.end(),
                                       " ", " ");
    if (!insc.empty() && insc[insc.length() - 1] == ',')
        insc.erase(insc.length() - 1);
    return insc;
}

void add_inscription(item_def &item, string inscrip)
{
    if (!item.inscription.empty())
    {
        if (ends_with(item.inscription, ","))
            item.inscription += " ";
        else
            item.inscription += ", ";
    }

    item.inscription += inscrip;
}

static const char* _jewellery_base_ability_description(int subtype)
{
    switch (subtype)
    {
    case RING_SUSTAIN_ABILITIES:
        return "It sustains your strength, intelligence and dexterity.";
    case RING_WIZARDRY:
        return "It improves your spell success rate.";
    case RING_FIRE:
        return "It enhances your fire magic, and weakens your ice magic.";
    case RING_ICE:
        return "It enhances your ice magic, and weakens your fire magic.";
    case RING_TELEPORTATION:
        return "It causes random teleportation, and can be evoked to teleport "
               "at will.";
    case RING_TELEPORT_CONTROL:
        return "It can be evoked for teleport control.";
    case AMU_CLARITY:
        return "It provides mental clarity.";
    case AMU_WARDING:
        return "It may prevent the melee attacks of summoned creatures.";
    case AMU_RESIST_CORROSION:
        return "It protects you from acid and corrosion.";
    case AMU_THE_GOURMAND:
        return "It allows you to eat raw meat even when not hungry.";
#if TAG_MAJOR_VERSION == 34
    case AMU_CONSERVATION:
        return "It protects your inventory from destruction.";
#endif
    case AMU_RESIST_MUTATION:
        return "It protects you from mutation.";
    case AMU_GUARDIAN_SPIRIT:
        return "It causes incoming damage to be split between your health and "
               "magic.";
    case AMU_FAITH:
        return "It allows you to gain divine favour quickly.";
    case AMU_STASIS:
        return "It prevents you from being teleported, slowed, hasted or "
               "paralysed.";
    case AMU_INACCURACY:
        return "It reduces the accuracy of all your attacks.";
    }
    return "";
}

struct property_descriptor
{
    artefact_prop_type property;
    const char* desc;           // If it contains %d, will be replaced by value.
    bool is_graded_resist;
};

static string _randart_descrip(const item_def &item)
{
    string description;

    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_desc_properties(item, proprt, known, true);

    const property_descriptor propdescs[] =
    {
        { ARTP_AC, "It affects your AC (%d).", false },
        { ARTP_EVASION, "It affects your evasion (%d).", false},
        { ARTP_STRENGTH, "It affects your strength (%d).", false},
        { ARTP_INTELLIGENCE, "It affects your intelligence (%d).", false},
        { ARTP_DEXTERITY, "It affects your dexterity (%d).", false},
        { ARTP_SLAYING, "It affects your accuracy and damage with ranged "
                        "weapons and melee attacks (%d).", false},
        { ARTP_FIRE, "fire", true},
        { ARTP_COLD, "cold", true},
        { ARTP_ELECTRICITY, "It insulates you from electricity.", false},
        { ARTP_POISON, "It protects you from poison.", false},
        { ARTP_NEGATIVE_ENERGY, "negative energy", true},
        { ARTP_SUSTAB, "It sustains your strength, intelligence and dexterity.", false},
        { ARTP_MAGIC, "It affects your resistance to hostile enchantments.", false},
        { ARTP_HP, "It affects your health (%d).", false},
        { ARTP_MAGICAL_POWER, "It affects your magic capacity (%d).", false},
        { ARTP_EYESIGHT, "It enhances your eyesight.", false},
        { ARTP_INVISIBLE, "It lets you turn invisible.", false},
        { ARTP_FLY, "It lets you fly.", false},
        { ARTP_BLINK, "It lets you blink.", false},
        { ARTP_BERSERK, "It lets you go berserk.", false},
        { ARTP_NOISES, "It may make noises in combat.", false},
        { ARTP_PREVENT_SPELLCASTING, "It prevents spellcasting.", false},
        { ARTP_CAUSE_TELEPORTATION, "It causes random teleportation.", false},
        { ARTP_PREVENT_TELEPORTATION, "It prevents most forms of teleportation.",
          false},
        { ARTP_ANGRY,  "It may make you go berserk in combat.", false},
        { ARTP_CURSED, "It may recurse itself when equipped.", false},
        { ARTP_CLARITY, "It protects you against confusion.", false},
        { ARTP_MUTAGENIC, "It causes magical contamination when unequipped.", false},
        { ARTP_RMSL, "It protects you from missiles.", false},
        { ARTP_FOG, "It can be evoked to emit clouds of fog.", false},
        { ARTP_REGENERATION, "It increases your rate of regeneration.", false},
        { ARTP_RCORR, "It protects you from acid and corrosion.", false},
        { ARTP_RMUT, "It protects you from mutation.", false},
    };

    // Give a short description of the base type, for base types with no
    // corresponding ARTP.
    if (item.base_type == OBJ_JEWELLERY
        && (item_ident(item, ISFLAG_KNOW_TYPE)))
    {
        const char* type = _jewellery_base_ability_description(item.sub_type);
        if (*type)
        {
            description += "\n";
            description += type;
        }
    }

    for (unsigned i = 0; i < ARRAYSZ(propdescs); ++i)
    {
        if (known_proprt(propdescs[i].property))
        {
            // Only randarts with ARTP_CURSED > 0 may recurse themselves.
            if (propdescs[i].property == ARTP_CURSED
                && proprt[propdescs[i].property] < 1)
            {
                continue;
            }

            string sdesc = propdescs[i].desc;

            // FIXME Not the nicest hack.
            char buf[80];
            snprintf(buf, sizeof buf, "%+d", proprt[propdescs[i].property]);
            sdesc = replace_all(sdesc, "%d", buf);

            if (propdescs[i].is_graded_resist)
            {
                int idx = proprt[propdescs[i].property] + 3;
                idx = min(idx, 6);
                idx = max(idx, 0);

                const char* prefixes[] =
                {
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

            description += '\n';
            description += sdesc;
        }
    }

    if (known_proprt(ARTP_STEALTH))
    {
        const int stval = proprt[ARTP_STEALTH];
        char buf[80];
        snprintf(buf, sizeof buf, "\nIt makes you %s%s stealthy.",
                 (stval < -1 || stval > 1) ? "much " : "",
                 (stval < 0) ? "less" : "more");
        description += buf;
    }

    return description;
}
#undef known_proprt

static const char *trap_names[] =
{
#if TAG_MAJOR_VERSION == 34
    "dart",
#endif
    "arrow", "spear",
#if TAG_MAJOR_VERSION > 34
    "teleport",
#endif
    "permanent teleport",
    "alarm", "blade",
    "bolt", "net", "Zot", "needle",
    "shaft", "passage", "pressure plate", "web",
#if TAG_MAJOR_VERSION == 34
    "gas", "teleport",
#endif
};

string trap_name(trap_type trap)
{
    COMPILE_CHECK(ARRAYSZ(trap_names) == NUM_TRAPS);

    if (trap >= 0 && trap < NUM_TRAPS)
        return trap_names[trap];
    return "";
}

string full_trap_name(trap_type trap)
{
    string basename = trap_name(trap);
    switch (trap)
    {
    case TRAP_GOLUBRIA:
        return basename + " of Golubria";
    case TRAP_PLATE:
    case TRAP_WEB:
    case TRAP_SHAFT:
        return basename;
    default:
        return basename + " trap";
    }
}

int str_to_trap(const string &s)
{
    // "Zot trap" is capitalised in trap_names[], but the other trap
    // names aren't.
    const string tspec = lowercase_string(s);

    // allow a couple of synonyms
    if (tspec == "random" || tspec == "any")
        return TRAP_RANDOM;

    for (int i = 0; i < NUM_TRAPS; ++i)
        if (tspec == lowercase_string(trap_names[i]))
            return i;

    return -1;
}

//---------------------------------------------------------------
//
// describe_demon
//
// Describes the random demons you find in Pandemonium.
//
//---------------------------------------------------------------
static string _describe_demon(const string& name, flight_type fly)
{
    const uint32_t seed = hash32(&name[0], name.length());
    #define HRANDOM_ELEMENT(arr, id) arr[hash_rand(ARRAYSZ(arr), seed, id)]

    const char* body_descs[] =
    {
        "huge, barrel-shaped ",
        "wispy, insubstantial ",
        "spindly ",
        "skeletal ",
        "horribly deformed ",
        "spiny ",
        "waif-like ",
        "scaly ",
        "sickeningly deformed ",
        "bruised and bleeding ",
        "sickly ",
        "mass of writhing tentacles for a ",
        "mass of ropey tendrils for a ",
        "tree trunk-like ",
        "hairy ",
        "furry ",
        "fuzzy ",
        "obese ",
        "fat ",
        "slimy ",
        "wrinkled ",
        "metallic ",
        "glassy ",
        "crystalline ",
        "muscular ",
        "icky ",
        "swollen ",
        "lumpy ",
        "armoured ",
        "carapaced ",
        "slender ",
    };

    const char* wing_names[] =
    {
        " with small insectoid wings",
        " with large insectoid wings",
        " with moth-like wings",
        " with butterfly wings",
        " with huge, bat-like wings",
        " with fleshy wings",
        " with small, bat-like wings",
        " with hairy wings",
        " with great feathered wings",
        " with shiny metal wings",
    };

    const char* lev_names[] =
    {
        " which hovers in mid-air",
        " with sacs of gas hanging from its back",
    };

    const char* nonfly_names[] =
    {
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
        " and a single huge eye in the centre of its forehead",
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

    const char* misc_descs[] =
    {
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

    ostringstream description;
    description << "A powerful demon, " << name << " has ";

    const string a_body = HRANDOM_ELEMENT(body_descs, 1);
    description << article_a(a_body) << "body";

    switch (fly)
    {
    case FL_WINGED:
        description << HRANDOM_ELEMENT(wing_names, 2);
        break;

    case FL_LEVITATE:
        description << HRANDOM_ELEMENT(lev_names, 2);
        break;

    case FL_NONE:  // does not fly
        if (hash_rand(4, seed, 3))
            description << HRANDOM_ELEMENT(nonfly_names, 2);
        break;
    }

    description << ".";

    if (hash_rand(40, seed, 4) < 3)
    {
        if (you.can_smell())
        {
            switch (hash_rand(3, seed, 5))
            {
            case 0:
                description << " It stinks of brimstone.";
                break;
            case 1:
                description << " It is surrounded by a sickening stench.";
                break;
            case 2:
                description << " It smells like rotting flesh"
                            << (you.species == SP_GHOUL ? " - yum!"
                                                       : ".");
                break;
            }
        }
    }
    else if (hash_rand(2, seed, 6))
        description << RANDOM_ELEMENT(misc_descs);

    return description.str();
}

void append_weapon_stats(string &description, const item_def &item)
{
    description += "\nBase accuracy: ";
    _append_value(description, property(item, PWPN_HIT), true);
    description += "  ";

    const int base_dam = property(item, PWPN_DAMAGE);
    const int ammo_type = fires_ammo_type(item);
    const int ammo_dam = ammo_type == MI_NONE ? 0 :
                                                ammo_type_damage(ammo_type);
    description += "Base damage: ";
    _append_value(description, base_dam + ammo_dam, false);
    description += "  ";

    description += "Base attack delay: ";
    _append_value(description, property(item, PWPN_SPEED) / 10.0f, false);
    description += "  ";

    description += "Minimum delay: ";
    _append_value(description, weapon_min_delay(item) / 10.0f, false);

    if (item_attack_skill(item) == SK_SLINGS)
    {
        description += make_stringf("\nFiring bullets:    Base damage: %d",
                                    base_dam +
                                    ammo_type_damage(MI_SLING_BULLET));
    }
}

static string _handedness_string(const item_def &item)
{
    string description;

    switch (you.hands_reqd(item))
    {
    case HANDS_ONE:
        if (you.species == SP_FORMICID)
            description += "It is a weapon for one hand-pair.";
        else
            description += "It is a one handed weapon.";
        break;
    case HANDS_TWO:
        description += "It is a two handed weapon.";
        break;
    }

    return description;
}

//---------------------------------------------------------------
//
// describe_weapon
//
//---------------------------------------------------------------
static string _describe_weapon(const item_def &item, bool verbose)
{
    string description;

    description.reserve(200);

    description = "";

    if (verbose)
    {
        description += "\n";
        append_weapon_stats(description, item);
    }

    const int spec_ench = (is_artefact(item) || verbose)
                          ? get_weapon_brand(item) : SPWPN_NORMAL;
    const int damtype = get_vorpal_type(item);

    if (verbose)
    {
        switch (item_attack_skill(item))
        {
        case SK_POLEARMS:
            description += "\n\nIt can be evoked to extend its reach.";
            break;
        case SK_AXES:
            description += "\n\nIt can hit multiple enemies in an arc"
                           " around the wielder.";
            break;
        case SK_SHORT_BLADES:
            // TODO: should we mention stabbing for "ok" stabbing weapons?
            // (long blades, piercing polearms, and clubs)
            {
                string adj = (item.sub_type == WPN_DAGGER) ? "extremely"
                                                           : "particularly";
                description += "\n\nIt is " + adj + " good for stabbing"
                               " unaware enemies.";
            }
            break;
        default:
            break;
        }
    }

    // special weapon descrip
    if (spec_ench != SPWPN_NORMAL && item_type_known(item))
    {
        description += "\n\n";

        switch (spec_ench)
        {
        case SPWPN_FLAMING:
            if (is_range_weapon(item))
            {
                description += "It turns projectiles fired from it into "
                    "bolts of flame.";
            }
            else
            {
                description += "It emits flame when wielded, causing extra "
                    "injury to most foes and up to double damage against "
                    "particularly susceptible opponents.";
                if (damtype == DVORP_SLICING || damtype == DVORP_CHOPPING)
                {
                    description += " Big, fiery blades are also staple "
                        "armaments of hydra-hunters.";
                }
            }
            break;
        case SPWPN_FREEZING:
            if (is_range_weapon(item))
            {
                description += "It turns projectiles fired from it into "
                    "bolts of frost.";
            }
            else
            {
                description += "It has been specially enchanted to freeze "
                    "those struck by it, causing extra injury to most foes "
                    "and up to double damage against particularly "
                    "susceptible opponents. It can also slow down "
                    "cold-blooded creatures.";
            }
            break;
        case SPWPN_HOLY_WRATH:
            description += "It has been blessed by the Shining One to "
                "cause great damage to the undead and demons.";
            break;
        case SPWPN_ELECTROCUTION:
            if (is_range_weapon(item))
            {
                description += "It charges the ammunition it shoots with "
                    "electricity; occasionally upon a hit, such missiles "
                    "may discharge and cause terrible harm.";
            }
            else
            {
                description += "Occasionally, upon striking a foe, it will "
                    "discharge some electrical energy and cause terrible "
                    "harm.";
            }
            break;
        case SPWPN_VENOM:
            if (is_range_weapon(item))
                description += "It poisons the ammo it fires.";
            else
                description += "It poisons the flesh of those it strikes.";
            break;
        case SPWPN_PROTECTION:
            description += "It protects the one who wields it against "
                "injury (+5 to AC).";
            break;
        case SPWPN_EVASION:
            description += "It affects your evasion (+5 to EV).";
            break;
        case SPWPN_DRAINING:
            description += "A truly terrible weapon, it drains the "
                "life of those it strikes.";
            break;
        case SPWPN_SPEED:
            description += "Attacks with this weapon are significantly faster.";
            break;
        case SPWPN_VORPAL:
            if (is_range_weapon(item))
            {
                description += "Any ";
                description += ammo_name(item);
                description += " fired from it inflicts extra damage.";
            }
            else
            {
                description += "It inflicts extra damage upon your "
                    "enemies.";
            }
            break;
        case SPWPN_CHAOS:
            if (is_range_weapon(item))
            {
                description += "Each time it fires, it turns the "
                    "launched projectile into a different, random type "
                    "of bolt.";
            }
            else
            {
                description += "Each time it hits an enemy it has a "
                    "different, random effect.";
            }
            break;
        case SPWPN_VAMPIRISM:
            description += "It inflicts no extra harm, but heals its "
                "wielder somewhat when it strikes a living foe.";
            break;
        case SPWPN_PAIN:
            description += "In the hands of one skilled in necromantic "
                "magic, it inflicts extra damage on living creatures.";
            break;
        case SPWPN_DISTORTION:
            description += "It warps and distorts space around it. "
                "Unwielding it can cause banishment or high damage.";
            break;
        case SPWPN_PENETRATION:
            description += "Ammo fired by it will pass through the "
                "targets it hits, potentially hitting all targets in "
                "its path until it reaches maximum range.";
            break;
        case SPWPN_REAPING:
            description += "If a monster killed with it leaves a "
                "corpse in good enough shape, the corpse will be "
                "animated as a zombie friendly to the killer.";
            break;
        case SPWPN_ANTIMAGIC:
            description += "It disrupts the flow of magical energy around "
                    "spellcasters and certain magical creatures (including "
                    "the wielder).";
            break;
        }
    }

    if (you.duration[DUR_WEAPON_BRAND] && &item == you.weapon())
    {
        description += "\nIt is temporarily rebranded; it is actually a";
        if ((int) you.props[ORIGINAL_BRAND_KEY] == SPWPN_NORMAL)
            description += "n unbranded weapon.";
        else
        {
            description += " weapon of "
                        + ego_type_string(item, false, you.props[ORIGINAL_BRAND_KEY])
                        + ".";
        }
    }

    if (is_artefact(item))
    {
        string rand_desc = _randart_descrip(item);
        if (!rand_desc.empty())
        {
            description += "\n";
            description += rand_desc;
        }

        // XXX: Can't happen, right?
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES)
            && item_type_known(item))
        {
            description += "\nThis weapon may have some hidden properties.";
        }
    }

    if (verbose)
    {
        description += "\n\nThis weapon falls into the";

        const skill_type skill = item_attack_skill(item);

        description +=
            make_stringf(" '%s' category. ",
                         skill == SK_FIGHTING ? "buggy" : skill_name(skill));

        description += _handedness_string(item);

        if (!you.could_wield(item, true))
            description += "\nIt is too large for you to wield.";
    }

    if (!is_artefact(item))
    {
        if (item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus >= MAX_WPN_ENCHANT)
            description += "\nIt cannot be enchanted further.";
        else
        {
            description += "\nIt can be maximally enchanted to +";
            _append_value(description, MAX_WPN_ENCHANT, false);
            description += ".";
        }
    }

    return description;
}

//---------------------------------------------------------------
//
// describe_ammo
//
//---------------------------------------------------------------
static string _describe_ammo(const item_def &item)
{
    string description;

    description.reserve(64);

    const bool can_launch = has_launcher(item);
    const bool can_throw  = is_throwable(&you, item, true);

    if (item.special && item_type_known(item))
    {
        description += "\n\n";

        string threw_or_fired;
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
            description += "It turns into a bolt of flame.";
            break;
        case SPMSL_FROST:
            description += "It turns into a bolt of frost.";
            break;
        case SPMSL_CHAOS:
            description += "When ";

            if (can_throw)
            {
                description += "thrown, ";
                if (can_launch)
                    description += "or ";
            }

            if (can_launch)
                description += "fired from an appropriate launcher, ";

            description += "it turns into a bolt of a random type.";
            break;
        case SPMSL_POISONED:
            description += "It is coated with poison.";
            break;
        case SPMSL_CURARE:
            description += "It is tipped with asphyxiating poison.";
            break;
        case SPMSL_PARALYSIS:
            description += "It is tipped with a paralysing substance.";
            break;
        case SPMSL_SLOW:
            description += "It is coated with a substance that causes slowness of the body.";
            break;
        case SPMSL_SLEEP:
            description += "It is coated with a fast-acting tranquilizer.";
            break;
        case SPMSL_CONFUSION:
            description += "It is tipped with a substance that causes confusion.";
            break;
#if TAG_MAJOR_VERSION == 34
        case SPMSL_SICKNESS:
            description += "It has been contaminated by something likely to cause disease.";
            break;
#endif
        case SPMSL_FRENZY:
            description += "It is tipped with a substance that causes a mindless "
                "rage, making people attack friend and foe alike.";
            break;
       case SPMSL_RETURNING:
            description += "A skilled user can throw it in such a way "
                "that it will return to its owner.";
            break;
        case SPMSL_PENETRATION:
            description += "It will pass through any targets it hits, "
                "potentially hitting all targets in its path until it "
                "reaches maximum range.";
            break;
        case SPMSL_DISPERSAL:
            description += "Any target it hits will blink, with a "
                "tendency towards blinking further away from the one "
                "who " + threw_or_fired + " it.";
            break;
        case SPMSL_EXPLODING:
            description += "It will explode into fragments upon "
                "hitting a target, hitting an obstruction, or reaching "
                "the end of its range.";
            break;
        case SPMSL_STEEL:
            description += "Compared to normal ammo, it does 30% more "
                "damage, is destroyed upon impact only 1/10th of the "
                "time.";
            break;
        case SPMSL_SILVER:
            description += "Silver sears all those touched by chaos. "
                "Compared to normal ammo, it does 75% more damage to "
                "chaotic and magically transformed beings. It also does "
                "extra damage against mutated beings according to how "
                "mutated they are. With due care, silver ammo can still "
                "be handled by those it affects.";
            break;
        }
    }

    append_missile_info(description, item);

    return description;
}

void append_armour_stats(string &description, const item_def &item)
{
    description += "\nBase armour rating: ";
    _append_value(description, property(item, PARM_AC), false);
    description += "       ";

    description += "Encumbrance rating: ";
    _append_value(description, -property(item, PARM_EVASION), false);
}

void append_missile_info(string &description, const item_def &item)
{
    const int dam = property(item, PWPN_DAMAGE);
    if (dam)
        description += make_stringf("\nBase damage: %d\n", dam);

    if (ammo_always_destroyed(item))
        description += "\nIt will always be destroyed on impact.";
    else if (!ammo_never_destroyed(item))
        description += "\nIt may be destroyed on impact.";
}

//---------------------------------------------------------------
//
// describe_armour
//
//---------------------------------------------------------------
static string _describe_armour(const item_def &item, bool verbose)
{
    string description;

    description.reserve(200);

    if (verbose
        && item.sub_type != ARM_SHIELD
        && item.sub_type != ARM_BUCKLER
        && item.sub_type != ARM_LARGE_SHIELD)
    {
        description += "\n";
        append_armour_stats(description, item);
    }

    const int ego = get_armour_ego_type(item);
    if (ego != SPARM_NORMAL && item_type_known(item) && verbose)
    {
        description += "\n\n";

        switch (ego)
        {
        case SPARM_RUNNING:
            if (item.sub_type == ARM_NAGA_BARDING)
                description += "It allows its wearer to slither at a great speed.";
            else
                description += "It allows its wearer to run at a great speed.";
            break;
        case SPARM_FIRE_RESISTANCE:
            description += "It protects its wearer from heat.";
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
        case SPARM_INVISIBILITY:
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
            description += "It is very cumbersome, thus slowing your movement.";
            break;
        case SPARM_FLYING:
            description += "It can be activated to allow its wearer to "
                "fly indefinitely.";
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
            description += "It increases the power of its wearer's "
                "magical spells.";
            break;
#if TAG_MAJOR_VERSION == 34
        case SPARM_PRESERVATION:
            description += "It does nothing special.";
            break;
#endif
        case SPARM_REFLECTION:
            description += "It reflects blocked things back in the "
                "direction they came from.";
            break;

        case SPARM_SPIRIT_SHIELD:
            description += "It shields its wearer from harm at the cost "
                "of magical power.";
            break;

        // This is only for gloves.
        case SPARM_ARCHERY:
            description += "It improves your effectiveness with ranged weaponry.";
            break;
        }
    }

    if (is_artefact(item))
    {
        string rand_desc = _randart_descrip(item);
        if (!rand_desc.empty())
        {
            description += "\n";
            description += rand_desc;
        }

        // Can't happen, right? (XXX)
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES) && item_type_known(item))
            description += "\nThis armour may have some hidden properties.";
    }

    if (!is_artefact(item))
    {
        const int max_ench = armour_max_enchant(item);
        if (armour_is_hide(item))
        {
            description += "\nEnchanting it will turn it into a suit of "
                           "magical armour.";
        }
        else if (item.plus < max_ench || !item_ident(item, ISFLAG_KNOW_PLUSES))
        {
            description += "\nIt can be maximally enchanted to +";
            _append_value(description, max_ench, false);
            description += ".";
        }
        else
            description += "\nIt cannot be enchanted further.";
    }

    return description;
}

//---------------------------------------------------------------
//
// describe_jewellery
//
//---------------------------------------------------------------
static string _describe_jewellery(const item_def &item, bool verbose)
{
    string description;

    description.reserve(200);

    if (verbose && !is_artefact(item)
        && item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        // Explicit description of ring power.
        if (item.plus != 0)
        {
            switch (item.sub_type)
            {
            case RING_PROTECTION:
                description += "\nIt affects your AC (";
                _append_value(description, item.plus, true);
                description += ").";
                break;

            case RING_EVASION:
                description += "\nIt affects your evasion (";
                _append_value(description, item.plus, true);
                description += ").";
                break;

            case RING_STRENGTH:
                description += "\nIt affects your strength (";
                _append_value(description, item.plus, true);
                description += ").";
                break;

            case RING_INTELLIGENCE:
                description += "\nIt affects your intelligence (";
                _append_value(description, item.plus, true);
                description += ").";
                break;

            case RING_DEXTERITY:
                description += "\nIt affects your dexterity (";
                _append_value(description, item.plus, true);
                description += ").";
                break;

            case RING_SLAYING:
                description += "\nIt affects your accuracy and damage "
                               "with ranged weapons and melee attacks (";
                _append_value(description, item.plus, true);
                description += ").";
                break;

            default:
                break;
            }
        }
    }

    // Artefact properties.
    if (is_artefact(item))
    {
        string rand_desc = _randart_descrip(item);
        if (!rand_desc.empty())
        {
            description += "\n";
            description += rand_desc;
        }
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES) ||
            !item_ident(item, ISFLAG_KNOW_TYPE))
        {
            description += "\nThis ";
            description += (jewellery_is_amulet(item) ? "amulet" : "ring");
            description += " may have hidden properties.";
        }
    }

    return description;
}

static bool _compare_card_names(card_type a, card_type b)
{
    return string(card_name(a)) < string(card_name(b));
}

//---------------------------------------------------------------
//
// describe_misc_item
//
//---------------------------------------------------------------
static bool _check_buggy_deck(const item_def &deck, string &desc)
{
    if (!is_deck(deck))
    {
        desc += "This isn't a deck at all!\n";
        return true;
    }

    const CrawlHashTable &props = deck.props;

    if (!props.exists("cards")
        || props["cards"].get_type() != SV_VEC
        || props["cards"].get_vector().get_type() != SV_BYTE
        || cards_in_deck(deck) == 0)
    {
        return true;
    }

    return false;
}

static string _describe_deck(const item_def &item)
{
    string description;

    description.reserve(100);

    description += "\n";

    if (_check_buggy_deck(item, description))
        return "";

    if (item_type_known(item))
        description += deck_contents(item.sub_type) + "\n";

    const vector<card_type> drawn_cards = get_drawn_cards(item);
    if (!drawn_cards.empty())
    {
        description += "Drawn card(s): ";
        for (unsigned int i = 0; i < drawn_cards.size(); ++i)
        {
            if (i != 0)
                description += ", ";
            description += card_name(drawn_cards[i]);
        }
        description += "\n";
    }

    const int num_cards = cards_in_deck(item);
    int last_known_card = -1;
    if (top_card_is_known(item))
    {
        description += "Next card(s): ";
        for (int i = 0; i < num_cards; ++i)
        {
            uint8_t flags;
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
        description += "\n";
    }

    // Marked cards which we don't know straight off.
    vector<card_type> marked_cards;
    for (int i = last_known_card + 1; i < num_cards; ++i)
    {
        uint8_t flags;
        const card_type card = get_card_and_flags(item, -i-1, flags);
        if (flags & CFLAG_MARKED)
            marked_cards.push_back(card);
    }
    if (!marked_cards.empty())
    {
        sort(marked_cards.begin(), marked_cards.end(),
                  _compare_card_names);

        description += "Marked card(s): ";
        for (unsigned int i = 0; i < marked_cards.size(); ++i)
        {
            if (i != 0)
                description += ", ";
            description += card_name(marked_cards[i]);
        }
        description += "\n";
    }

    // Seen cards in the deck.
    vector<card_type> seen_cards;
    for (int i = 0; i < num_cards; ++i)
    {
        uint8_t flags;
        const card_type card = get_card_and_flags(item, -i-1, flags);

        // This *might* leak a bit of information...oh well.
        if ((flags & CFLAG_SEEN) && !(flags & CFLAG_MARKED))
            seen_cards.push_back(card);
    }
    if (!seen_cards.empty())
    {
        sort(seen_cards.begin(), seen_cards.end(),
                  _compare_card_names);

        description += "Seen card(s): ";
        for (unsigned int i = 0; i < seen_cards.size(); ++i)
        {
            if (i != 0)
                description += ", ";
            description += card_name(seen_cards[i]);
        }
        description += "\n";
    }

    return description;
}

// Adds a list of all spells contained in a book or rod to its
// description string.
void append_spells(string &desc, const item_def &item)
{
    if (!item.has_spells())
        return;

    desc += "\n\nSpells                             Type                      Level\n";

    for (int j = 0; j < SPELLBOOK_SIZE; ++j)
    {
        spell_type stype = which_spell_in_book(item, j);
        if (stype == SPELL_NO_SPELL)
            continue;

        string name = (is_memorised(stype) ? "*" : "");
                    name += spell_title(stype);
        desc += chop_string(name, 35);

        string schools;
        if (item.base_type == OBJ_RODS)
            schools = "Evocations";
        else
            schools = spell_schools_string(stype);

        desc += chop_string(schools, 65 - 36);

        desc += make_stringf("%d\n", spell_difficulty(stype));
    }
}

// ========================================================================
//      Public Functions
// ========================================================================

bool is_dumpable_artefact(const item_def &item)
{
    return is_known_artefact(item) && item_ident(item, ISFLAG_KNOW_PROPERTIES);
}

//---------------------------------------------------------------
//
// get_item_description
//
//---------------------------------------------------------------
string get_item_description(const item_def &item, bool verbose,
                            bool dump, bool noquote)
{
    if (dump)
        noquote = true;

    ostringstream description;

    if (!dump)
    {
        string name = item.name(DESC_INVENTORY_EQUIP);
        if (!in_inventory(item))
            name = uppercase_first(name);
        description << name << ".";
    }

#ifdef DEBUG_DIAGNOSTICS
    if (!dump)
    {
        description << setfill('0');
        description << "\n\n"
                    << "base: " << static_cast<int>(item.base_type)
                    << " sub: " << static_cast<int>(item.sub_type)
                    << " plus: " << item.plus << " plus2: " << item.plus2
                    << " special: " << item.special
                    << "\n"
                    << "quant: " << item.quantity
                    << " flags: " << hex << setw(8) << item.flags
                    << dec << "\n"
                    << "x: " << item.pos.x << " y: " << item.pos.y
                    << " link: " << item.link
                    << " slot: " << item.slot
                    << " ident_type: "
                    << static_cast<int>(get_ident_type(item))
                    << " book_number: " << item.book_number()
                    << "\nannotate: "
                    << stash_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item);
    }
#endif

    if (verbose || (item.base_type != OBJ_WEAPONS
                    && item.base_type != OBJ_ARMOUR
                    && item.base_type != OBJ_BOOKS))
    {
        description << "\n\n";

        bool need_base_desc = true;

        if (dump)
        {
            description << "["
                        << item.name(DESC_DBNAME, true, false, false)
                        << "]";
            need_base_desc = false;
        }
        else if (is_unrandom_artefact(item) && item_type_known(item))
        {
            const string desc = getLongDescription(get_artefact_name(item));
            if (!desc.empty())
            {
                description << desc;
                need_base_desc = false;
                description.seekp((streamoff)-1, ios_base::cur);
                description << " ";
            }
        }
        // Randart jewellery properties will be listed later,
        // just describe artefact status here.
        else if (is_artefact(item) && item_type_known(item)
                 && item.base_type == OBJ_JEWELLERY)
        {
            description << "It is an ancient artefact.";
            need_base_desc = false;
        }

        if (need_base_desc)
        {
            string db_name = item.name(DESC_DBNAME, true, false, false);
            string db_desc = getLongDescription(db_name);

            if (db_desc.empty())
            {
                if (item_type_known(item))
                {
                    description << "[ERROR: no desc for item name '" << db_name
                                << "']\n";
                }
                else
                {
                    description << uppercase_first(item.name(DESC_A, true,
                                                             false, false));
                    description << ".\n";
                }
            }
            else
                description << db_desc;

            // Get rid of newline at end of description; in most cases we
            // will be adding "\n\n" immediately, and we want only one,
            // not two, blank lines.  This allow allows the "unpleasant"
            // message for chunks to appear on the same line.
            description.seekp((streamoff)-1, ios_base::cur);
            description << " ";
        }
    }

    bool need_extra_line = true;
    string desc;
    switch (item.base_type)
    {
    // Weapons, armour, jewellery, books might be artefacts.
    case OBJ_WEAPONS:
        desc = _describe_weapon(item, verbose);
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_ARMOUR:
        desc = _describe_armour(item, verbose);
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_JEWELLERY:
        desc = _describe_jewellery(item, verbose);
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_BOOKS:
        if (!player_can_memorise_from_spellbook(item))
        {
            description << "\nThis book is beyond your current level of "
                           "understanding.";

            if (!item_type_known(item))
                break;
        }

        if (!verbose
            && (Options.dump_book_spells || is_random_artefact(item)))
        {
            append_spells(desc, item);
            if (desc.empty())
                need_extra_line = false;
            else
                description << desc;
        }
        break;

    case OBJ_MISSILES:
        description << _describe_ammo(item);
        break;

    case OBJ_WANDS:
    {
        const bool known_empty = is_known_empty_wand(item);

        if (!item_ident(item, ISFLAG_KNOW_PLUSES) && !known_empty)
        {
            description << "\nIf evoked without being fully identified,"
                           " several charges will be wasted out of"
                           " unfamiliarity with the device.";
        }


        if (item_type_known(item))
        {
            const int max_charges = wand_max_charges(item.sub_type);
            if (item.charges < max_charges
                || !item_ident(item, ISFLAG_KNOW_PLUSES))
            {
                description << "\nIt can have at most " << max_charges
                            << " charges.";
            }
            else
                description << "\nIt is fully charged.";
        }

        if (known_empty)
            description << "\nUnfortunately, it has no charges left.";
        break;
    }

    case OBJ_CORPSES:
        if (item.sub_type == CORPSE_SKELETON)
            break;
        // intentional fall-through
    case OBJ_FOOD:
        if (item.base_type == OBJ_FOOD)
        {
            description << "\n\n";

            const int turns = food_turns(item);
            ASSERT(turns > 0);
            if (turns > 1)
            {
                description << "It is large enough that eating it takes "
                            << ((turns > 2) ? "several" : "a couple of")
                            << " turns, during which time the eater is vulnerable"
                               " to attack.";
            }
            else
                description << "It is small enough that eating it takes "
                               "only one turn.";
        }
        if (item.base_type == OBJ_CORPSES || item.sub_type == FOOD_CHUNK)
        {
            switch (determine_chunk_effect(item, true))
            {
            case CE_POISONOUS:
                description << "\n\nThis meat is poisonous.";
                break;
            case CE_MUTAGEN:
                description << "\n\nEating this meat will cause random "
                               "mutations.";
                break;
            case CE_ROT:
                description << "\n\nEating this meat will cause rotting.";
                break;
            default:
                break;
            }
        }
        break;

    case OBJ_RODS:
        if (verbose)
        {
            description <<
                "\nIt uses its own magic reservoir for casting spells, and "
                "recharges automatically according to the recharging "
                "rate.";

            const int max_charges = MAX_ROD_CHARGE;
            const int max_recharge_rate = MAX_WPN_ENCHANT;
            if (item_ident(item, ISFLAG_KNOW_PLUSES))
            {
                const int num_charges = item.charge_cap / ROD_CHARGE_MULT;
                if (max_charges > num_charges)
                {
                    description << "\nIt can currently hold " << num_charges
                                << " charges. It can be magically "
                                << "recharged to contain up to "
                                << max_charges << " charges.";
                }
                else
                    description << "\nIts capacity can be increased no further.";

                const int recharge_rate = item.special;
                if (recharge_rate < max_recharge_rate)
                {
                    description << "\nIts current recharge rate is "
                                << (recharge_rate >= 0 ? "+" : "")
                                << recharge_rate << ". It can be magically "
                                << "recharged up to +" << max_recharge_rate
                                << ".";
                }
                else
                    description << "\nIts recharge rate is at maximum.";
            }
            else
            {
                description << "\nIt can have at most " << max_charges
                            << " charges and +" << max_recharge_rate
                            << " recharge rate.";
            }
        }
        else if (Options.dump_book_spells)
        {
            append_spells(desc, item);
            if (desc.empty())
                need_extra_line = false;
            else
                description << desc;
        }

        {
            string stats = "\n";
            append_weapon_stats(stats, item);
            description << stats;
        }
        description << "\n\nIt falls into the 'Maces & Flails' category.";
        break;

    case OBJ_STAVES:
        {
            string stats = "\n";
            append_weapon_stats(stats, item);
            description << stats;
        }
        description << "\n\nIt falls into the 'Staves' category. ";
        description << _handedness_string(item);
        break;

    case OBJ_MISCELLANY:
        if (is_deck(item))
            description << _describe_deck(item);
        if (is_xp_evoker(item))
        {
            description << "\nOnce released, the spirits within this device "
                           "will dissipate, leaving it inert, though new ones "
                           "may be attracted as its bearer battles through the "
                           "dungeon and grows in power and wisdom.";

            if (!evoker_is_charged(item))
            {
                description << "\n\nThe device is presently inert.";
                if (evoker_is_charging(item))
                    description << " Gaining experience will recharge it.";
                else if (in_inventory(item))
                {
                    description << " Another item of the same type is"
                                   " currently charging.";
                }
            }
        }
        break;

    case OBJ_POTIONS:
#ifdef DEBUG_BLOOD_POTIONS
        // List content of timer vector for blood potions.
        if (!dump && is_blood_potion(item))
        {
            item_def stack = static_cast<item_def>(item);
            CrawlHashTable &props = stack.props;
            ASSERT(props.exists("timer"));
            CrawlVector &timer = props["timer"].get_vector();
            ASSERT(!timer.empty());

            description << "\nQuantity: " << stack.quantity
                        << "        Timer size: " << (int) timer.size();
            description << "\nTimers:\n";
            for (int i = 0; i < timer.size(); ++i)
                 description << (timer[i].get_int()) << "  ";
        }
#endif

    case OBJ_SCROLLS:
    case OBJ_ORBS:
    case OBJ_GOLD:
        // No extra processing needed for these item types.
        break;

    default:
        die("Bad item class");
    }

    if (!verbose && item_known_cursed(item))
        description << "\nIt has a curse placed upon it.";
    else
    {
        if (verbose)
        {
            if (need_extra_line)
                description << "\n";
            if (item_known_cursed(item))
                description << "\nIt has a curse placed upon it.";

            if (is_artefact(item))
            {
                if (item.base_type == OBJ_ARMOUR
                    || item.base_type == OBJ_WEAPONS)
                {
                    description << "\nThis ancient artefact cannot be changed "
                        "by magic or mundane means.";
                }
                // Randart jewellery has already displayed this line.
                else if (item.base_type != OBJ_JEWELLERY
                         || (item_type_known(item) && is_unrandom_artefact(item)))
                {
                    description << "\nIt is an ancient artefact.";
                }
            }
        }
    }

    if (conduct_type ct = good_god_hates_item_handling(item))
    {
        description << "\n\n" << uppercase_first(god_name(you.religion))
                    << " opposes the use of such an ";

        if (ct == DID_NECROMANCY)
            description << "evil";
        else
            description << "unholy";

        description << " item.";
    }
    else if (god_hates_item_handling(item))
    {
        description << "\n\n" << uppercase_first(god_name(you.religion))
                    << " disapproves of the use of such an item.";
    }

    if (verbose && origin_describable(item))
        description << "\n" << origin_desc(item) << ".";

    // This information is obscure and differs per-item, so looking it up in
    // a docs file you don't know to exist is tedious.  On the other hand,
    // it breaks the screen for people on very small terminals.
    if (verbose && get_number_of_lines() >= 28)
    {
        description << "\n\n" << "Stash search prefixes: "
                    << userdef_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item);
        string menu_prefix = item_prefix(item, false);
        if (!menu_prefix.empty())
            description << "\nMenu/colouring prefixes: " << menu_prefix;
    }

    if (verbose && !noquote && (!item_type_known(item) || !is_random_artefact(item)))
    {
        const unsigned int lineWidth = get_number_of_cols();
        const          int height    = get_number_of_lines();
        string quote;
        if (is_unrandom_artefact(item) && item_type_known(item))
            quote = getQuoteString(get_artefact_name(item));
        else
            quote = getQuoteString(item.name(DESC_DBNAME, true, false, false));

        if (count_desc_lines(description.str(), lineWidth)
            + count_desc_lines(quote, lineWidth) < height)
        {
            if (!quote.empty())
                description << "\n\n" << quote;
        }
    }

    return description.str();
}

void get_feature_desc(const coord_def &pos, describe_info &inf)
{
    dungeon_feature_type feat = env.map_knowledge(pos).feat();

    string desc      = feature_description_at(pos, false, DESC_A, false);
    string db_name   = feat == DNGN_ENTER_SHOP ? "a shop" : desc;
    string long_desc = getLongDescription(db_name);

    inf.title = uppercase_first(desc);
    if (!ends_with(desc, ".") && !ends_with(desc, "!")
        && !ends_with(desc, "?"))
    {
        inf.title += ".";
    }

    // If we couldn't find a description in the database then see if
    // the feature's base name is different.
    if (long_desc.empty())
    {
        db_name   = feature_description_at(pos, false, DESC_A, false, true);
        long_desc = getLongDescription(db_name);
    }

    const string marker_desc =
        env.markers.property_at(pos, MAT_ANY, "feature_description_long");

    // suppress this if the feature changed out of view
    if (!marker_desc.empty() && grd(pos) == feat)
        long_desc += marker_desc;

    // Display branch descriptions on the entries to those branches.
    if (feat_is_stair(feat))
    {
        for (branch_iterator it; it; ++it)
        {
            if (it->entry_stairs == feat)
            {
                long_desc += "\n";
                long_desc += getLongDescription(it->shortname);
                break;
            }
        }
    }

    // mention the ability to pray at altars
    if (feat_is_altar(feat))
        long_desc += "(Pray here to learn more.)";

    inf.body << long_desc;

    inf.quote = getQuoteString(db_name);
}

// Returns the pressed key in key
static int _print_toggle_message(const describe_info &inf, int& key)
{
    if (inf.quote.empty())
    {
        mouse_control mc(MOUSE_MODE_MORE);
        key = getchm();
        return false;
    }
    else
    {
        const int bottom_line = min(30, get_number_of_lines());
        cgotoxy(1, bottom_line);
        formatted_string::parse_string(
            "Press '<w>!</w>'"
#ifdef USE_TILE_LOCAL
            " or <w>Right-click</w>"
#endif
            " to toggle between the description and quote.").display();

        mouse_control mc(MOUSE_MODE_MORE);
        key = getchm();

        if (key == '!' || key == CK_MOUSE_CMD)
            return true;

        return false;
    }
}

void describe_feature_wide(const coord_def& pos, bool show_quote)
{
    describe_info inf;
    get_feature_desc(pos, inf);

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_feature");
#endif

    if (show_quote)
        _print_quote(inf);
    else
        print_description(inf);

    if (crawl_state.game_is_hints())
        hints_describe_pos(pos.x, pos.y);

    int key;
    if (_print_toggle_message(inf, key))
        describe_feature_wide(pos, !show_quote);
}

void get_item_desc(const item_def &item, describe_info &inf)
{
    // Don't use verbose descriptions if the item contains spells,
    // so we can actually output these spells if space is scarce.
    const bool verbose = !item.has_spells();
    inf.body << get_item_description(item, verbose, false, true);
}

// Returns true if spells can be shown to player.
static bool _can_show_spells(const item_def &item)
{
    return item.has_spells()
           && (item.base_type != OBJ_BOOKS || item_type_known(item)
               || player_can_memorise_from_spellbook(item));
}

static void _show_spells(const item_def &item)
{
    formatted_string fs;
    item_def dup = item;
    spellbook_contents(dup, &fs);
    fs.display(2, -2);
}

static void _show_item_description(const item_def &item)
{
    const unsigned int lineWidth = get_number_of_cols() - 1;
    const          int height    = get_number_of_lines();

    string desc = get_item_description(item, true, false,
                                       crawl_state.game_is_hints_tutorial());

    const int num_lines = count_desc_lines(desc, lineWidth) + 1;

    // XXX: hack: Leave room for "Inscribe item?" and the blank line above
    // it by removing item quote.  This should really be taken care of
    // by putting the quotes into a separate DB and treating them as
    // a suffix that can be ignored by print_description().
    if (height - num_lines <= 2)
        desc = get_item_description(item, true, false, true);

    print_description(desc);
    if (crawl_state.game_is_hints())
        hints_describe_item(item);

    if (_can_show_spells(item))
      _show_spells(item);
}

static bool _describe_spells(const item_def &item)
{
    const int c = getchm();
    if (c < 'a' || c > 'h')     //jmf: was 'g', but 8=h
    {
        clear_messages();
        return false;
    }

    const int spell_index = letter_to_index(c);

    const spell_type nthing = which_spell_in_book(item, spell_index);
    if (nthing == SPELL_NO_SPELL)
        return false;

    describe_spell(nthing, &item);
    return item.is_valid();
}

static bool _can_memorise(item_def &item)
{
    return item.base_type == OBJ_BOOKS && in_inventory(item)
           && player_can_memorise_from_spellbook(item);
}

static void _update_inscription(item_def &item)
{
    if (item.base_type == OBJ_BOOKS && in_inventory(item)
        && !_can_memorise(item))
    {
        inscribe_book_highlevel(item);
    }
}

static bool _describe_spellbook(item_def &item)
{
    while (true)
    {
        // Memorised spell while reading a spellbook.
        if (already_learning_spell(-1))
            return false;

        _show_item_description(item);
        _update_inscription(item);

        cgotoxy(1, wherey());
        textcolour(LIGHTGREY);

        if (_can_memorise(item) && !crawl_state.player_is_dead())
        {
            cprintf("Select a spell to read its description, to "
                    "memorise it or to forget it.");
        }
        else
            cprintf("Select a spell to read its description.");

        if (_describe_spells(item))
            continue;

        return true;
    }
}

// it takes a key and a list of commands and it returns
// the command from the list which corresponds to the key
static command_type _get_action(int key, vector<command_type> actions)
{
    static bool act_key_init = true; // Does act_key needs to be initialise?
    static map<command_type, int> act_key;
    if (act_key_init)
    {
        act_key[CMD_WIELD_WEAPON]       = 'w';
        act_key[CMD_UNWIELD_WEAPON]     = 'u';
        act_key[CMD_QUIVER_ITEM]        = 'q';
        act_key[CMD_WEAR_ARMOUR]        = 'w';
        act_key[CMD_REMOVE_ARMOUR]      = 't';
        act_key[CMD_EVOKE]              = 'v';
        act_key[CMD_EAT]                = 'e';
        act_key[CMD_READ]               = 'r';
        act_key[CMD_WEAR_JEWELLERY]     = 'p';
        act_key[CMD_REMOVE_JEWELLERY]   = 'r';
        act_key[CMD_QUAFF]              = 'q';
        act_key[CMD_DROP]               = 'd';
        act_key[CMD_INSCRIBE_ITEM]      = 'i';
        act_key[CMD_ADJUST_INVENTORY]   = '=';
        act_key_init = false;
    }

    for (vector<command_type>::const_iterator at = actions.begin();
         at < actions.end(); ++at)
    {
        if (key == act_key[*at])
            return *at;
    }
    return CMD_NO_CMD;
}

//---------------------------------------------------------------
//
// _actions_prompt
//
// print a list of actions to be performed on the item
static bool _actions_prompt(item_def &item, bool allow_inscribe, bool do_prompt)
{
#ifdef USE_TILE_LOCAL
    PrecisionMenu menu;
    TextItem* tmp = NULL;
    MenuFreeform* freeform = new MenuFreeform();
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    freeform->init(coord_def(1, 1),
                   coord_def(get_number_of_cols(), get_number_of_lines()),
                   "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0, 0), coord_def(0, 0), "highlighter");
    menu.attach_object(highlighter);
#endif
    string prompt = "You can ";
    int keyin;
    vector<command_type> actions;
    actions.push_back(CMD_ADJUST_INVENTORY);
    if (item_equip_slot(item) == EQ_WEAPON)
        actions.push_back(CMD_UNWIELD_WEAPON);
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_RODS:
    case OBJ_MISCELLANY:
        if (!item_is_equipped(item))
        {
            if (item_is_wieldable(item))
                actions.push_back(CMD_WIELD_WEAPON);
            if (is_throwable(&you, item))
                actions.push_back(CMD_QUIVER_ITEM);
        }
        break;
    case OBJ_MISSILES:
        if (you.species != SP_FELID)
            actions.push_back(CMD_QUIVER_ITEM);
        break;
    case OBJ_ARMOUR:
        if (item_is_equipped(item))
            actions.push_back(CMD_REMOVE_ARMOUR);
        else
            actions.push_back(CMD_WEAR_ARMOUR);
        break;
    case OBJ_FOOD:
        if (can_eat(item, true, false))
            actions.push_back(CMD_EAT);
        break;
    case OBJ_SCROLLS:
    case OBJ_BOOKS: // only unknown ones
        if (item.sub_type != BOOK_MANUAL)
            actions.push_back(CMD_READ);
        break;
    case OBJ_JEWELLERY:
        if (item_is_equipped(item))
            actions.push_back(CMD_REMOVE_JEWELLERY);
        else
            actions.push_back(CMD_WEAR_JEWELLERY);
        break;
    case OBJ_POTIONS:
        if (you.undead_state() != US_UNDEAD) // mummies and lich form forbidden
            actions.push_back(CMD_QUAFF);
        break;
    default:
        ;
    }
#if defined(CLUA_BINDINGS)
    if (clua.callbooleanfn(false, "ch_item_wieldable", "i", &item))
        actions.push_back(CMD_WIELD_WEAPON);
#endif

    if (item_is_evokable(item))
        actions.push_back(CMD_EVOKE);

    actions.push_back(CMD_DROP);

    if (allow_inscribe)
        actions.push_back(CMD_INSCRIBE_ITEM);

    static bool act_str_init = true; // Does act_str needs to be initialised?
    static map<command_type, string> act_str;
    if (act_str_init)
    {
        act_str[CMD_WIELD_WEAPON]       = "(w)ield";
        act_str[CMD_UNWIELD_WEAPON]     = "(u)nwield";
        act_str[CMD_QUIVER_ITEM]        = "(q)uiver";
        act_str[CMD_WEAR_ARMOUR]        = "(w)ear";
        act_str[CMD_REMOVE_ARMOUR]      = "(t)ake off";
        act_str[CMD_EVOKE]              = "e(v)oke";
        act_str[CMD_EAT]                = "(e)at";
        act_str[CMD_READ]               = "(r)ead";
        act_str[CMD_WEAR_JEWELLERY]     = "(p)ut on";
        act_str[CMD_REMOVE_JEWELLERY]   = "(r)emove";
        act_str[CMD_QUAFF]              = "(q)uaff";
        act_str[CMD_DROP]               = "(d)rop";
        act_str[CMD_INSCRIBE_ITEM]      = "(i)nscribe";
        act_str[CMD_ADJUST_INVENTORY]   = "(=)adjust";
        act_str_init = false;
    }

    for (vector<command_type>::const_iterator at = actions.begin();
         at < actions.end(); ++at)
    {
#ifdef USE_TILE_LOCAL
        tmp = new TextItem();
        tmp->set_id(*at);
        tmp->set_text(act_str[*at]);
        tmp->set_fg_colour(CYAN);
        tmp->set_bg_colour(BLACK);
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text(act_str[*at]);
        tmp->set_bounds(coord_def(prompt.size() + 1, wherey()),
                        coord_def(prompt.size() + act_str[*at].size() + 1,
                                  wherey() + 1));
        freeform->attach_item(tmp);
        tmp->set_visible(true);
#endif
        prompt += act_str[*at];
        if (at < actions.end() - 2)
            prompt += ", ";
        else if (at == actions.end() - 2)
            prompt += " or ";
    }
    prompt += " the " + item.name(DESC_BASENAME) + ".";

    prompt = "<cyan>" + prompt + "</cyan>";
    if (do_prompt)
        formatted_string::parse_string(prompt).display();

#ifdef TOUCH_UI

    //draw menu over the top of the prompt text
    tiles.get_crt()->attach_menu(&menu);
    freeform->set_visible(true);
    highlighter->set_visible(true);
    menu.draw_menu();
#endif

    keyin = toalower(getch_ck());
    command_type action = _get_action(keyin, actions);

#ifdef TOUCH_UI
    if (menu.process_key(keyin))
    {
        vector<MenuItem*> selection = menu.get_selected_items();
        if (selection.size() == 1)
            action = (command_type) selection.at(0)->get_id();
    }
#endif

    const int slot = item.link;
    ASSERT_RANGE(slot, 0, ENDOFPACK);

    switch (action)
    {
    case CMD_WIELD_WEAPON:
        redraw_screen();
        wield_weapon(true, slot);
        return false;
    case CMD_UNWIELD_WEAPON:
        redraw_screen();
        wield_weapon(true, SLOT_BARE_HANDS);
        return false;
    case CMD_QUIVER_ITEM:
        redraw_screen();
        quiver_item(slot);
        return false;
    case CMD_WEAR_ARMOUR:
        redraw_screen();
        wear_armour(slot);
        return false;
    case CMD_REMOVE_ARMOUR:
        redraw_screen();
        takeoff_armour(slot);
        return false;
    case CMD_EVOKE:
        redraw_screen();
        evoke_item(slot);
        return false;
    case CMD_EAT:
        redraw_screen();
        eat_food(slot);
        return false;
    case CMD_READ:
        if (item.base_type != OBJ_BOOKS || item.sub_type == BOOK_DESTRUCTION)
            redraw_screen();
        read_scroll(slot);
        // In case of a book, stay in the inventory to see the content.
        return item.base_type == OBJ_BOOKS && item.sub_type != BOOK_DESTRUCTION;
    case CMD_WEAR_JEWELLERY:
        redraw_screen();
        puton_ring(slot);
        return false;
    case CMD_REMOVE_JEWELLERY:
        redraw_screen();
        remove_ring(slot, true);
        return false;
    case CMD_QUAFF:
        redraw_screen();
        drink(slot);
        return false;
    case CMD_DROP:
        redraw_screen();
        drop_item(slot, you.inv[slot].quantity);
        return false;
    case CMD_INSCRIBE_ITEM:
        inscribe_item(item, false);
        break;
    case CMD_ADJUST_INVENTORY:
        _adjust_item(item);
        return false;
    case CMD_NO_CMD:
    default:
        return true;
    }
    return true;
}

//---------------------------------------------------------------
//
// describe_item
//
// Describes all items in the game.
// Returns false if we should break out of the inventory loop.
//---------------------------------------------------------------
bool describe_item(item_def &item, bool allow_inscribe, bool shopping)
{
    if (!item.defined())
        return true;

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_item");
#endif

    if (_can_show_spells(item))
        return _describe_spellbook(item);

    _show_item_description(item);
    _update_inscription(item);

    if (allow_inscribe && crawl_state.game_is_tutorial())
        allow_inscribe = false;

    // Don't ask if we're dead.
    if (in_inventory(item) && crawl_state.prev_cmd != CMD_RESISTS_SCREEN
        && !(you.dead || crawl_state.updating_scores))
    {
        // Don't draw the prompt if there aren't enough rows left.
        const bool do_prompt = wherey() <= get_number_of_lines() - 2;
        if (do_prompt)
            cgotoxy(1, wherey() + 2);
        return _actions_prompt(item, allow_inscribe, do_prompt);
    }
    else
        getchm();

    return true;
}

static void _safe_newline()
{
    if (wherey() == get_number_of_lines())
    {
        cgotoxy(1, wherey());
        formatted_string::parse_string(string(80, ' ')).display();
        cgotoxy(1, wherey());
    }
    else
        formatted_string::parse_string("\n").display();
}

// There are currently two ways to inscribe an item:
// * using the inscribe command ('{') -> msgwin = true
// * from the inventory when viewing an item -> msgwin = false
//
// msgwin also controls whether a hints mode explanation can be
// shown, or whether the pre- and post-inscription item names need to be
// printed.
void inscribe_item(item_def &item, bool msgwin)
{
    if (msgwin)
        mprf_nocap(MSGCH_EQUIPMENT, "%s", item.name(DESC_INVENTORY).c_str());

    const bool is_inscribed = !item.inscription.empty();
    string prompt = is_inscribed ? "Replace inscription with what? "
                                 : "Inscribe with what? ";

    char buf[79];
    int ret;
    if (msgwin)
        ret = msgwin_get_line(prompt, buf, sizeof buf, NULL, item.inscription);
    else
    {
        _safe_newline();
        prompt = "<cyan>" + prompt + "</cyan>";
        formatted_string::parse_string(prompt).display();
        ret = cancellable_get_line(buf, sizeof buf, NULL, NULL,
                                  item.inscription);
    }

    if (ret)
    {
        if (msgwin)
            canned_msg(MSG_OK);
        return;
    }

    // Strip spaces from the end.
    for (int i = strlen(buf) - 1; i >= 0; --i)
    {
        if (isspace(buf[i]))
            buf[i] = 0;
        else
            break;
    }

    if (item.inscription == buf)
    {
        if (msgwin)
            canned_msg(MSG_OK);
        return;
    }

    item.inscription = buf;

    if (msgwin)
    {
        mprf_nocap(MSGCH_EQUIPMENT, "%s", item.name(DESC_INVENTORY).c_str());
        you.wield_change  = true;
        you.redraw_quiver = true;
    }
}

static void _adjust_item(item_def &item)
{
    _safe_newline();
    string prompt = "<cyan>Adjust to which letter? </cyan>";
    formatted_string::parse_string(prompt).display();
    const int keyin = getch_ck();

    if (isaalpha(keyin))
    {
        const int a = letter_to_index(item.slot);
        const int b = letter_to_index(keyin);
        swap_inv_slots(a,b,true);
    }
}

static void _append_spell_stats(const spell_type spell,
                                string &description,
                                bool rod)
{
    if (rod)
    {
        snprintf(info, INFO_SIZE,
                 "\nLevel: %d",
                 spell_difficulty(spell));
    }
    else
    {
        const string schools = spell_schools_string(spell);
        char* failure = failure_rate_to_string(spell_fail(spell));
        snprintf(info, INFO_SIZE,
                 "\nLevel: %d        School%s: %s        Fail: %s",
                 spell_difficulty(spell),
                 schools.find("/") != string::npos ? "s" : "",
                 schools.c_str(),
                 failure);
        free(failure);
    }
    description += info;
    description += "\n\nPower : ";
    description += spell_power_string(spell, rod);
    description += "\nRange : ";
    description += spell_range_string(spell, rod);
    description += "\nHunger: ";
    description += spell_hunger_string(spell, rod);
    description += "\nNoise : ";
    description += spell_noise_string(spell);
    description += "\n";
}

// Returns BOOK_MEM if you can memorise the spell BOOK_FORGET if you can
// forget it and BOOK_NEITHER if you can do neither
static int _get_spell_description(const spell_type spell,
                                   string &description,
                                   const item_def* item = NULL)
{
    description.reserve(500);

    description  = spell_title(spell);
    description += "\n\n";
    const string long_descrip = getLongDescription(string(spell_title(spell))
                                                   + " spell");

    if (!long_descrip.empty())
        description += long_descrip;
    else
    {
        description += "This spell has no description. "
                       "Casting it may therefore be unwise. "
#ifdef DEBUG
                       "Instead, go fix it. ";
#else
                       "Please file a bug report.";
#endif
    }

    // Report summon cap
    if (const int limit = summons_limit(spell))
    {
        description += "You can sustain at most " + number_in_words(limit)
                       + " creature" + (limit > 1 ? "s" : "") + " summoned by this spell.\n";
    }

    const bool rod = item && item->base_type == OBJ_RODS;

    if (god_hates_spell(spell, you.religion, rod))
    {
        description += uppercase_first(god_name(you.religion))
                       + " frowns upon the use of this spell.\n";
        if (god_loathes_spell(spell, you.religion))
            description += "You'd be excommunicated if you dared to cast it!\n";
    }
    else if (god_likes_spell(spell, you.religion))
    {
        description += uppercase_first(god_name(you.religion))
                       + " supports the use of this spell.\n";
    }
    if (item && !player_can_memorise_from_spellbook(*item))
    {
        description += "The spell is scrawled in ancient runes that are beyond "
                       "your current level of understanding.\n";
    }
    if (spell_is_useless(spell) && you_can_memorise(spell))
    {
        description += "This spell will have no effect right now: "
                        + spell_uselessness_reason(spell) + "\n";
    }

    _append_spell_stats(spell, description, rod);

    if (crawl_state.player_is_dead())
        return BOOK_NEITHER;

    const string quote = getQuoteString(string(spell_title(spell)) + " spell");
    if (!quote.empty())
        description += "\n" + quote;

    if (!you_can_memorise(spell))
        description += "\n" + desc_cannot_memorise_reason(spell) + "\n";

    if (item && item->base_type == OBJ_BOOKS && in_inventory(*item))
    {
        if (you.has_spell(spell))
        {
            description += "\n(F)orget this spell by destroying the book.\n";
            if (you_worship(GOD_SIF_MUNA))
                description +="Sif Muna frowns upon the destroying of books.\n";
            return BOOK_FORGET;
        }
        else if (player_can_memorise_from_spellbook(*item)
                 && you_can_memorise(spell))
        {
            description += "\n(M)emorise this spell.\n";
            return BOOK_MEM;
        }
    }

    return BOOK_NEITHER;
}

void get_spell_desc(const spell_type spell, describe_info &inf)
{
    string desc;
    _get_spell_description(spell, desc);
    inf.body << desc;
}

//---------------------------------------------------------------
//
// describe_spell
//
// Describes (most) every spell in the game.
//
//---------------------------------------------------------------
void describe_spell(spell_type spelled, const item_def* item)
{
#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_spell");
#endif

    string desc;
    const int mem_or_forget = _get_spell_description(spelled, desc, item);
    print_description(desc);

    mouse_control mc(MOUSE_MODE_MORE);
    char ch;
    if ((ch = getchm()) == 0)
        ch = getchm();

    if (mem_or_forget == BOOK_MEM && toupper(ch) == 'M')
    {
        redraw_screen();
        if (!learn_spell(spelled) || !you.turn_is_over)
            more();
        redraw_screen();
    }
    else if (mem_or_forget == BOOK_FORGET && toupper(ch) == 'F')
    {
        redraw_screen();
        if (!forget_spell_from_book(spelled, item) || !you.turn_is_over)
            more();
        redraw_screen();
    }
}

static string _describe_draconian(const monster_info& mi)
{
    string description;
    const int subsp = mi.draco_or_demonspawn_subspecies();

    if (subsp != mi.type)
    {
        description += "It has ";

        switch (subsp)
        {
        case MONS_BLACK_DRACONIAN:      description += "black ";   break;
        case MONS_MOTTLED_DRACONIAN:    description += "mottled "; break;
        case MONS_YELLOW_DRACONIAN:     description += "yellow ";  break;
        case MONS_GREEN_DRACONIAN:      description += "green ";   break;
        case MONS_PURPLE_DRACONIAN:     description += "purple ";  break;
        case MONS_RED_DRACONIAN:        description += "red ";     break;
        case MONS_WHITE_DRACONIAN:      description += "white ";   break;
        case MONS_GREY_DRACONIAN:       description += "grey ";    break;
        case MONS_PALE_DRACONIAN:       description += "pale ";    break;
        default:
            break;
        }

        description += "scales. ";
    }

    switch (subsp)
    {
    case MONS_BLACK_DRACONIAN:
        description += "Sparks flare out of its mouth and nostrils.";
        break;
    case MONS_MOTTLED_DRACONIAN:
        description += "Liquid flames drip from its mouth.";
        break;
    case MONS_YELLOW_DRACONIAN:
        description += "Acidic fumes swirl around it.";
        break;
    case MONS_GREEN_DRACONIAN:
        description += "Venom drips from its jaws.";
        break;
    case MONS_PURPLE_DRACONIAN:
        description += "Its outline shimmers with magical energy.";
        break;
    case MONS_RED_DRACONIAN:
        description += "Smoke pours from its nostrils.";
        break;
    case MONS_WHITE_DRACONIAN:
        description += "Frost pours from its nostrils.";
        break;
    case MONS_GREY_DRACONIAN:
        description += "Its scales and tail are adapted to the water.";
        break;
    case MONS_PALE_DRACONIAN:
        description += "It is cloaked in a pall of superheated steam.";
        break;
    default:
        break;
    }

    return description;
}

static string _describe_chimera(const monster_info& mi)
{
    string description = "It has the head of ";

    description += apply_description(DESC_A, get_monster_data(mi.base_type)->name);

    monster_type part2 = get_chimera_part(&mi,2);
    description += ", the head of ";
    if (part2 == mi.base_type)
    {
        description += "another ";
        description += apply_description(DESC_PLAIN,
                                         get_monster_data(part2)->name);
    }
    else
        description += apply_description(DESC_A, get_monster_data(part2)->name);

    monster_type part3 = get_chimera_part(&mi,3);
    description += ", and the head of ";
    if (part3 == mi.base_type || part3 == part2)
    {
        if (part2 == mi.base_type)
            description += "yet ";
        description += "another ";
        description += apply_description(DESC_PLAIN,
                                         get_monster_data(part3)->name);
    }
    else
        description += apply_description(DESC_A, get_monster_data(part3)->name);

    description += ". It has the body of ";
    description += apply_description(DESC_A,
                                     get_monster_data(mi.base_type)->name);

    const bool has_wings = mi.props.exists("chimera_batty")
                           || mi.props.exists("chimera_wings");
    if (mi.props.exists("chimera_legs"))
    {
        const monster_type leggy_part =
            get_chimera_part(&mi, mi.props["chimera_legs"].get_int());
        if (has_wings)
            description += ", ";
        else
            description += ", and ";
        description += "the legs of ";
        description += apply_description(DESC_A,
                                         get_monster_data(leggy_part)->name);
    }

    if (has_wings)
    {
        monster_type wing_part = mi.props.exists("chimera_batty") ?
            get_chimera_part(&mi, mi.props["chimera_batty"].get_int())
            : get_chimera_part(&mi, mi.props["chimera_wings"].get_int());

        switch (mons_class_flies(wing_part))
        {
        case FL_WINGED:
            description += " and the wings of ";
            break;
        case FL_LEVITATE:
            description += " and it hovers like ";
            break;
        case FL_NONE:
            description += " and it moves like "; // Unseen horrors
            break;
        }
        description += apply_description(DESC_A,
                                         get_monster_data(wing_part)->name);
    }
    description += ".";
    return description;
}

static string _describe_demonspawn_role(monster_type type)
{
    switch (type)
    {
    case MONS_BLOOD_SAINT:
        return "It weaves powerful and unpredictable spells of devastation.";
    case MONS_CHAOS_CHAMPION:
        return "It possesses chaotic, reality-warping powers.";
    case MONS_WARMONGER:
        return "It is devoted to combat, disrupting the magic of its foes as "
               "it battles endlessly.";
    case MONS_CORRUPTER:
        return "It corrupts space around itself, and can twist even the very "
               "flesh of its opponents.";
    case MONS_BLACK_SUN:
        return "It shines with an unholy radiance, and wields powers of "
               "darkness from its devotion to the deities of death.";
    default:
        return "";
    }
}

static string _describe_demonspawn_base(int species)
{
    switch (species)
    {
    case MONS_MONSTROUS_DEMONSPAWN:
        return "It is more beast now than whatever species it is descended from.";
    case MONS_GELID_DEMONSPAWN:
        return "It is covered in icy armour.";
    case MONS_INFERNAL_DEMONSPAWN:
        return "It gives off an intense heat.";
    case MONS_PUTRID_DEMONSPAWN:
        return "It is surrounded by sickly fumes and gases.";
    case MONS_TORTUROUS_DEMONSPAWN:
        return "It menaces with bony spines.";
    }
    return "";
}

static string _describe_demonspawn(const monster_info& mi)
{
    string description;
    const int subsp = mi.draco_or_demonspawn_subspecies();

    description += _describe_demonspawn_base(subsp);

    if (subsp != mi.type)
    {
        const string demonspawn_role = _describe_demonspawn_role(mi.type);
        if (!demonspawn_role.empty())
            description += " " + demonspawn_role;
    }

    return description;
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
    case MR_RES_ROTTING:
        return "rotting";
    case MR_RES_NEG:
        return "negative energy";
    default:
        return "buggy resistance";
    }
}

static const char* _get_threat_desc(mon_threat_level_type threat)
{
    switch (threat)
    {
    case MTHRT_TRIVIAL: return "harmless";
    case MTHRT_EASY:    return "easy";
    case MTHRT_TOUGH:   return "dangerous";
    case MTHRT_NASTY:   return "extremely dangerous";
    case MTHRT_UNDEF:
    default:            return "buggily threatening";
    }
}

static const char* _describe_attack_flavour(attack_flavour flavour)
{
    switch (flavour)
    {
    case AF_ACID:            return "deal extra acid damage";
    case AF_BLINK:           return "blink self";
    case AF_COLD:            return "deal extra cold damage";
    case AF_CONFUSE:         return "cause confusion";
    case AF_DRAIN_STR:       return "drain strength";
    case AF_DRAIN_INT:       return "drain intelligence";
    case AF_DRAIN_DEX:       return "drain dexterity";
    case AF_DRAIN_STAT:      return "drain strength, intelligence or dexterity";
    case AF_DRAIN_XP:        return "drain skills";
    case AF_ELEC:            return "cause electrocution";
    case AF_FIRE:            return "deal extra fire damage";
    case AF_HUNGER:          return "cause hungering";
    case AF_MUTATE:          return "cause mutations";
    case AF_PARALYSE:        return "cause paralysis";
    case AF_POISON:          return "cause poisoning";
    case AF_POISON_STRONG:   return "cause strong poisoning through resistance";
    case AF_ROT:             return "cause rotting";
    case AF_VAMPIRIC:        return "drain health";
    case AF_KLOWN:           return "cause random powerful effects";
    case AF_DISTORT:         return "cause wild translocation effects";
    case AF_RAGE:            return "cause berserking";
    case AF_STICKY_FLAME:    return "apply sticky flame";
    case AF_CHAOS:           return "cause unpredictable effects";
    case AF_STEAL:           return "steal items";
    case AF_CRUSH:           return "constrict";
    case AF_REACH:           return "deal damage from a distance";
    case AF_HOLY:            return "deal extra damage to undead and demons";
    case AF_ANTIMAGIC:       return "drain magic";
    case AF_PAIN:            return "cause pain to the living";
    case AF_ENSNARE:         return "ensnare with webbing";
    case AF_ENGULF:          return "engulf with water";
    case AF_PURE_FIRE:       return "deal pure fire damage";
    case AF_DRAIN_SPEED:     return "drain speed";
    case AF_VULN:            return "reduce resistance to hostile enchantments";
    case AF_WEAKNESS_POISON: return "cause poison and weakness";
    case AF_SHADOWSTAB:      return "deal extra damage from the shadows";
    case AF_DROWN:           return "deal drowning damage";
    case AF_FIREBRAND:       return "deal extra fire damage and surround the defender with flames";
    case AF_CORRODE:         return "corrode armour";
    case AF_SCARAB:          return "reduce negative energy resistance and drain health, speed, or skills";
    default:                 return "";
    }
}

static string _monster_attacks_description(const monster_info& mi)
{
    ostringstream result;
    vector<attack_flavour> attack_flavours;
    vector<string> attack_descs;
    // Weird attack types that act like attack flavours.
    bool trample = false;
    bool reach_sting = false;

    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
    {
        attack_flavour af = mi.attack[i].flavour;

        bool match = false;
        for (unsigned int k = 0; k < attack_flavours.size(); ++k)
            if (attack_flavours[k] == af)
                match = true;

        if (!match)
        {
            attack_flavours.push_back(af);
            const char* desc = _describe_attack_flavour(af);
            if (*desc)
                attack_descs.push_back(desc);
        }

        if (mi.attack[i].type == AT_TRAMPLE)
            trample = true;

        if (mi.attack[i].type == AT_REACH_STING)
            reach_sting = true;
    }

    if (trample)
        attack_descs.push_back("knock back the defender");

    // Assumes nothing has both AT_REACH_STING and AF_REACH.
    if (reach_sting)
        attack_descs.push_back(_describe_attack_flavour(AF_REACH));

    if (!attack_descs.empty())
    {
        result << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE));
        result << " may attack to " << comma_separated_line(attack_descs.begin(), attack_descs.end());
        result << ".\n";
    }

    return result.str();
}

static string _monster_spell_type_description(const monster_info& mi,
                                              mon_spell_slot_flags flags,
                                              string set_name,
                                              string desc_singular,
                                              string desc_plural)
{
    unique_books books = get_unique_spells(mi, flags);
    const size_t num_books = books.size();

    if (num_books == 0)
        return "";

    ostringstream result;

    result << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE));

    if (num_books > 1)
        result << desc_plural;
    else
        result << desc_singular;

    // Loop through books and display spells/abilities for each of them
    for (size_t i = 0; i < num_books; ++i)
    {
        vector<spell_type> &book_spells = books[i];

        // Display spells for this book
        if (num_books > 1)
            result << set_name << " " << i+1 << ": ";

        for (size_t j = 0; j < book_spells.size(); ++j)
        {
            const spell_type spell = book_spells[j];
            if (j > 0)
                result << ", ";
            result << spell_title(spell);
        }
        result << "\n";
    }

    return result.str();
}

static string _monster_spells_description(const monster_info& mi)
{
    // Show a generic message for pan lords, since they're secret.
    if (mi.type == MONS_PANDEMONIUM_LORD)
        return "It may possess any of a vast number of diabolical powers.\n";

    // Show monster spells and spell-like abilities.
    if (!mi.has_spells())
        return "";

    ostringstream result;

    result << _monster_spell_type_description(
        mi, MON_SPELL_NATURAL, "Set",
        " possesses the following special abilities: ",
        " possesses one of the following sets of special abilities:\n");
    result << _monster_spell_type_description(
        mi, MON_SPELL_MAGICAL, "Set",
        " possesses the following magical abilities: ",
        " possesses one of the following sets of magical abilities:\n");
    result << _monster_spell_type_description(
        mi, MON_SPELL_DEMONIC, "Set",
        " possesses the following demonic abilities: ",
        " possesses one of the following sets of demonic abilities:\n");
    result << _monster_spell_type_description(
        mi, MON_SPELL_PRIEST, "Set",
        " possesses the following divine abilities: ",
        " possesses one of the following sets of divine abilities:\n");
    result << _monster_spell_type_description(
        mi, MON_SPELL_WIZARD, "Book",
        " has mastered the following spells: ",
        " has mastered one of the following spellbooks:\n");

    return result.str();
}

static const char *_speed_description(int speed)
{
    // These thresholds correspond to the player mutations for fast and slow.
    ASSERT(speed != 10);
    if (speed < 7)
        return "extremely slowly";
    else if (speed < 8)
        return "very slowly";
    else if (speed < 10)
        return "slowly";
    else if (speed > 15)
        return "extremely quickly";
    else if (speed > 13)
        return "very quickly";
    else if (speed > 10)
        return "quickly";

    return "buggily";
}

static void _add_energy_to_string(int speed, int energy, string what,
                                  vector<string> &fast, vector<string> &slow)
{
    if (energy == 10)
        return;

    const int act_speed = (speed * 10) / energy;
    if (act_speed > 10)
        fast.push_back(what + " " + _speed_description(act_speed));
    if (act_speed < 10)
        slow.push_back(what + " " + _speed_description(act_speed));
}


/**
 * Print a bar of +s and .s representing a given stat to a provided stream.
 *
 * @param value[in]         The current value represented by the bar.
 * @param max[in]           The max value that can be represented by the bar.
 * @param scale[in]         The value that each + and . represents.
 * @param name              The name of the bar.
 * @param result[in,out]    The stringstream to append to.
 * @param base_value[in]    The 'base' value represented by the bar. If
 *                          INT_MAX, is ignored.
 */
static void _print_bar(int value, int max, int scale,
                       string name, ostringstream &result,
                       int base_value = INT_MAX)
{
    if (base_value == INT_MAX)
        base_value = value;

    result << name << " ";

    const int cur_bars = value / scale;
    const int base_bars = base_value / scale;
    const int bars = cur_bars ? cur_bars : base_bars;
    const int max_bars = max / scale;

    const bool currently_disabled = !cur_bars && base_bars;

    if (currently_disabled)
        result << "(";

    for (int i = 0; i < min(bars, max_bars); i++)
        result << "+";

    if (currently_disabled)
        result << ")";

    for (int i = max_bars - 1; i >= bars; --i)
        result << ".";

#ifdef DEBUG_DIAGNOSTICS
    result << " (" << value << ")";
#endif

    if (currently_disabled)
    {
        result << " (Normal " << name << ")";

#ifdef DEBUG_DIAGNOSTICS
        result << " (" << base_value << ")";
#endif
    }

    result << "\n";
}

/**
 * Append information about a given monster's AC to the provided stream.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
static void _describe_monster_ac(const monster_info& mi, ostringstream &result)
{
    // max ac 40 (dispater)
    _print_bar(mi.ac, 40, 5, "AC", result);
}

/**
 * Append information about a given monster's EV to the provided stream.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
static void _describe_monster_ev(const monster_info& mi, ostringstream &result)
{
    // max ev 30 (eresh) (also to make space for parens)
    _print_bar(mi.ev, 30, 5, "EV", result, mi.base_ev);
}

/**
 * Append information about a given monster's MR to the provided stream.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
static void _describe_monster_mr(const monster_info& mi, ostringstream &result)
{
    if (mi.res_magic() == MAG_IMMUNE)
    {
        result << "MR ∞";
        return;
    }

    const int max_mr = 200; // export this? is this already exported?
    const int bar_scale = MR_PIP;
    _print_bar(mi.res_magic(), max_mr, bar_scale, "MR", result);
}


// Describe a monster's (intrinsic) resistances, speed and a few other
// attributes.
static string _monster_stat_description(const monster_info& mi)
{
    ostringstream result;

    _describe_monster_ac(mi, result);
    _describe_monster_ev(mi, result);
    _describe_monster_mr(mi, result);

    result << "\n";

    resists_t resist = mi.resists();

    const mon_resist_flags resists[] =
    {
        MR_RES_ELEC,    MR_RES_POISON, MR_RES_FIRE,
        MR_RES_STEAM,   MR_RES_COLD,   MR_RES_ACID,
        MR_RES_ROTTING, MR_RES_NEG,
    };

    vector<string> extreme_resists;
    vector<string> high_resists;
    vector<string> base_resists;
    vector<string> suscept;

    for (unsigned int i = 0; i < ARRAYSZ(resists); ++i)
    {
        int level = get_resist(resist, resists[i]);

        if (level != 0)
        {
            const char* attackname = _get_resist_name(resists[i]);
            level = max(level, -1);
            level = min(level,  3);
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

    vector<string> resist_descriptions;
    if (!extreme_resists.empty())
    {
        const string tmp = "immune to "
            + comma_separated_line(extreme_resists.begin(),
                                   extreme_resists.end());
        resist_descriptions.push_back(tmp);
    }
    if (!high_resists.empty())
    {
        const string tmp = "very resistant to "
            + comma_separated_line(high_resists.begin(), high_resists.end());
        resist_descriptions.push_back(tmp);
    }
    if (!base_resists.empty())
    {
        const string tmp = "resistant to "
            + comma_separated_line(base_resists.begin(), base_resists.end());
        resist_descriptions.push_back(tmp);
    }

    const char* pronoun = mi.pronoun(PRONOUN_SUBJECTIVE);

    if (mi.threat != MTHRT_UNDEF)
    {
        result << uppercase_first(pronoun) << " looks "
               << _get_threat_desc(mi.threat) << ".\n";
    }

    if (!resist_descriptions.empty())
    {
        result << uppercase_first(pronoun) << " is "
               << comma_separated_line(resist_descriptions.begin(),
                                       resist_descriptions.end(),
                                       "; and ", "; ")
               << ".\n";
    }

    // Is monster susceptible to anything? (On a new line.)
    if (!suscept.empty())
    {
        result << uppercase_first(pronoun) << " is susceptible to "
               << comma_separated_line(suscept.begin(), suscept.end())
               << ".\n";
    }

    if (mons_class_flag(mi.type, M_STATIONARY)
        && !mons_is_tentacle_or_tentacle_segment(mi.type))
    {
        result << uppercase_first(pronoun) << " cannot move.\n";
    }

    // Monsters can glow from both light and radiation.
    if (mons_class_flag(mi.type, M_GLOWS_LIGHT))
        result << uppercase_first(pronoun) << " is outlined in light.\n";
    if (mons_class_flag(mi.type, M_GLOWS_RADIATION))
        result << uppercase_first(pronoun) << " is glowing with mutagenic radiation.\n";
    if (mons_class_flag(mi.type, M_SHADOW))
        result << uppercase_first(pronoun) << " is wreathed in shadows.\n";

    // Seeing invisible.
    if (mons_class_flag(mi.type, M_SEE_INVIS)
            || mons_is_ghost_demon(mi.type) && mi.u.ghost.can_sinv)
    {
        result << uppercase_first(pronoun) << " can see invisible.\n";
    }

    // Echolocation, wolf noses, jellies, etc
    if (!mons_can_be_blinded(mi.type))
        result << uppercase_first(pronoun) << " is immune to blinding.\n";
    // XXX: could mention "immune to dazzling" here, but that's spammy, since
    // it's true of such a huge number of monsters. (undead, statues, plants).
    // Might be better to have some place where players can see holiness &
    // information about holiness.......?


    // Unusual monster speed.
    const int speed = mi.base_speed();
    bool did_speed = false;
    if (speed != 10 && speed != 0)
    {
        did_speed = true;
        result << uppercase_first(pronoun) << " is " << mi.speed_description();
    }
    const mon_energy_usage def = DEFAULT_ENERGY;
    if (!(mi.menergy == def))
    {
        const mon_energy_usage me = mi.menergy;
        vector<string> fast, slow;
        if (!did_speed)
            result << uppercase_first(pronoun) << " ";
        _add_energy_to_string(speed, me.move, "covers ground", fast, slow);
        // since MOVE_ENERGY also sets me.swim
        if (me.swim != me.move)
            _add_energy_to_string(speed, me.swim, "swims", fast, slow);
        _add_energy_to_string(speed, me.attack, "attacks", fast, slow);
        _add_energy_to_string(speed, me.missile, "shoots", fast, slow);
        _add_energy_to_string(
            speed, me.spell,
            mi.is_actual_spellcaster() ? "casts spells" :
            mi.is_priest()             ? "uses invocations"
                                       : "uses natural abilities", fast, slow);
        _add_energy_to_string(speed, me.special, "uses special abilities",
                              fast, slow);
        _add_energy_to_string(speed, me.item, "uses items", fast, slow);

        if (speed >= 10)
        {
            if (did_speed && fast.size() == 1)
                result << " and " << fast[0];
            else if (!fast.empty())
            {
                if (did_speed)
                    result << ", ";
                result << comma_separated_line(fast.begin(), fast.end());
            }
            if (!slow.empty())
            {
                if (did_speed || !fast.empty())
                    result << ", but ";
                result << comma_separated_line(slow.begin(), slow.end());
            }
        }
        else if (speed < 10)
        {
            if (did_speed && slow.size() == 1)
                result << " and " << slow[0];
            else if (!slow.empty())
            {
                if (did_speed)
                    result << ", ";
                result << comma_separated_line(slow.begin(), slow.end());
            }
            if (!fast.empty())
            {
                if (did_speed || !slow.empty())
                    result << ", but ";
                result << comma_separated_line(fast.begin(), fast.end());
            }
        }
        result << ".\n";
    }
    else if (did_speed)
        result << ".\n";

    // Can the monster fly, and how?
    // This doesn't give anything away since no (very) ugly things can
    // fly, all ghosts can fly, and for demons it's already mentioned in
    // their flavour description.
    switch (mi.fly)
    {
    case FL_NONE:
        break;
    case FL_WINGED:
        result << uppercase_first(pronoun) << " can fly.\n";
        break;
    case FL_LEVITATE:
        result << uppercase_first(pronoun) << " can fly magically.\n";
        break;
    }

    // Unusual regeneration rates.
    if (!mi.can_regenerate())
        result << uppercase_first(pronoun) << " cannot regenerate.\n";
    else if (mons_class_fast_regen(mi.type))
        result << uppercase_first(pronoun) << " regenerates quickly.\n";

    // Size
    static const char * const sizes[] =
    {
        "tiny",
        "very small",
        "small",
        NULL,     // don't display anything for 'medium'
        "large",
        "very large",
        "giant",
    };
    COMPILE_CHECK(ARRAYSZ(sizes) == NUM_SIZE_LEVELS);

    if (sizes[mi.body_size()])
    {
        result << uppercase_first(pronoun) << " is "
        << sizes[mi.body_size()] << ".\n";
    }

    result << _monster_attacks_description(mi);
    result << _monster_spells_description(mi);

    return result.str();
}

static string _serpent_of_hell_flavour(monster_type m)
{
    switch (m)
    {
    case MONS_SERPENT_OF_HELL_COCYTUS:
        return "cocytus";
    case MONS_SERPENT_OF_HELL_DIS:
        return "dis";
    case MONS_SERPENT_OF_HELL_TARTARUS:
        return "tartarus";
    default:
        return "gehenna";
    }
}

// Fetches the monster's database description and reads it into inf.
void get_monster_db_desc(const monster_info& mi, describe_info &inf,
                         bool &has_stat_desc, bool force_seen)
{
    if (inf.title.empty())
        inf.title = getMiscString(mi.common_name(DESC_DBNAME) + " title");
    if (inf.title.empty())
        inf.title = uppercase_first(mi.full_name(DESC_A, true)) + ".";

    string db_name;

    if (mi.props.exists("dbname"))
        db_name = mi.props["dbname"].get_string();
    else if (mi.mname.empty())
        db_name = mi.db_name();
    else
        db_name = mi.full_name(DESC_PLAIN, true);

    if (mons_species(mi.type) == MONS_SERPENT_OF_HELL)
        db_name += " " + _serpent_of_hell_flavour(mi.type);

    // This is somewhat hackish, but it's a good way of over-riding monsters'
    // descriptions in Lua vaults by using MonPropsMarker. This is also the
    // method used by set_feature_desc_long, etc. {due}
    if (!mi.description.empty())
        inf.body << mi.description;
    // Don't get description for player ghosts.
    else if (mi.type != MONS_PLAYER_GHOST
             && mi.type != MONS_PLAYER_ILLUSION)
    {
        inf.body << getLongDescription(db_name);
    }

    // And quotes {due}
    if (!mi.quote.empty())
        inf.quote = mi.quote;
    else
        inf.quote = getQuoteString(db_name);

    string symbol;
    symbol += get_monster_data(mi.type)->basechar;
    if (isaupper(symbol[0]))
        symbol = "cap-" + symbol;

    string quote2;
    if (!mons_is_unique(mi.type))
    {
        string symbol_prefix = "__" + symbol + "_prefix";
        inf.prefix = getLongDescription(symbol_prefix);

        string symbol_suffix = "__" + symbol + "_suffix";
        quote2 = getQuoteString(symbol_suffix);
    }

    if (!inf.quote.empty() && !quote2.empty())
        inf.quote += "\n";
    inf.quote += quote2;

    switch (mi.type)
    {
    case MONS_VAMPIRE:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAMPIRE_MAGE:
        if (you.undead_state() == US_ALIVE && mi.attitude == ATT_HOSTILE)
            inf.body << "\nIt wants to drink your blood!\n";
        break;

    case MONS_REAPER:
        if (you.undead_state(false) == US_ALIVE && mi.attitude == ATT_HOSTILE)
            inf.body <<  "\nIt has come for your soul!\n";
        break;

    case MONS_RED_DRACONIAN:
    case MONS_WHITE_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_PALE_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_GREY_DRACONIAN:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_KNIGHT:
    {
        inf.body << "\n" << _describe_draconian(mi) << "\n";
        break;
    }

    case MONS_MONSTROUS_DEMONSPAWN:
    case MONS_GELID_DEMONSPAWN:
    case MONS_INFERNAL_DEMONSPAWN:
    case MONS_PUTRID_DEMONSPAWN:
    case MONS_TORTUROUS_DEMONSPAWN:
    case MONS_BLOOD_SAINT:
    case MONS_CHAOS_CHAMPION:
    case MONS_WARMONGER:
    case MONS_CORRUPTER:
    case MONS_BLACK_SUN:
    {
        inf.body << "\n" << _describe_demonspawn(mi) << "\n";
        break;
    }

    case MONS_PLAYER_GHOST:
        inf.body << "The apparition of " << get_ghost_description(mi) << ".\n";
        break;

    case MONS_PLAYER_ILLUSION:
        inf.body << "An illusion of " << get_ghost_description(mi) << ".\n";
        break;

    case MONS_PANDEMONIUM_LORD:
        inf.body << _describe_demon(mi.mname, mi.fly) << "\n";
        break;

    case MONS_CHIMERA:
        inf.body << "\n" << _describe_chimera(mi) << "\n";
        break;

    case MONS_PROGRAM_BUG:
        inf.body << "If this monster is a \"program bug\", then it's "
                "recommended that you save your game and reload.  Please report "
                "monsters who masquerade as program bugs or run around the "
                "dungeon without a proper description to the authorities.\n";
        break;

    default:
        break;
    }

    if (!mons_is_unique(mi.type))
    {
        string symbol_suffix = "__";
        symbol_suffix += symbol;
        symbol_suffix += "_suffix";

        string suffix = getLongDescription(symbol_suffix);
              suffix += getLongDescription(symbol_suffix + "_examine");

        if (!suffix.empty())
            inf.body << "\n" << suffix;
    }

    // Get information on resistances, speed, etc.
    string result = _monster_stat_description(mi);
    if (!result.empty())
    {
        inf.body << "\n" << result;
        has_stat_desc = true;
    }

    bool stair_use = false;
    if (!mons_class_can_use_stairs(mi.type))
    {
        inf.body << "\n" << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE))
                 << " is incapable of using stairs.\n";
        stair_use = true;
    }

    if (mi.intel() <= I_PLANT)
    {
        inf.body << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE))
                 << " is mindless.\n";
    }
    else if (mi.intel() <= I_INSECT && you_worship(GOD_ELYVILON))
    {
        inf.body << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE))
                 << " is not intelligent enough to pacify.\n";
    }


    if (mi.is(MB_CHAOTIC))
    {
        inf.body << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE))
                 << " is vulnerable to silver and hated by Zin.\n";
    }

    if (mi.is(MB_SUMMONED))
    {
        inf.body << "\n" << "This monster has been summoned, and is thus only "
                       "temporary. Killing it yields no experience, nutrition "
                       "or items";
        if (!stair_use && mi.is(MB_SUMMONED_NO_STAIRS))
            inf.body << ", and it is incapable of using stairs";
        inf.body << ".\n";
    }

    if (mi.is(MB_PERM_SUMMON))
    {
        inf.body << "\n" << "This monster has been summoned in a durable "
                       "way, and only partially exists. Killing it yields no "
                       "experience, nutrition or items. You cannot easily "
                       "abjure it, though.\n";
    }

    if (mi.is(MB_SUMMONED_CAPPED))
    {
        inf.body << "\n" << "You have summoned too many monsters of this kind "
                            "to sustain them all, and thus this one will "
                            "shortly expire.\n";
    }

    if (!inf.quote.empty())
        inf.quote += "\n";

#ifdef DEBUG_DIAGNOSTICS
    if (mi.pos.origin() || !monster_at(mi.pos))
        return; // not a real monster
    monster& mons = *monster_at(mi.pos);

    if (mons.has_originating_map())
    {
        inf.body << make_stringf("\nPlaced by map: %s",
                                 mons.originating_map().c_str());
    }

    inf.body << "\nMonster health: "
             << mons.hit_points << "/" << mons.max_hit_points << "\n";

    const actor *mfoe = mons.get_foe();
    inf.body << "Monster foe: "
             << (mfoe? mfoe->name(DESC_PLAIN, true)
                 : "(none)");

    vector<string> attitude;
    if (mons.friendly())
        attitude.push_back("friendly");
    if (mons.neutral())
        attitude.push_back("neutral");
    if (mons.good_neutral())
        attitude.push_back("good_neutral");
    if (mons.strict_neutral())
        attitude.push_back("strict_neutral");
    if (mons.pacified())
        attitude.push_back("pacified");
    if (mons.wont_attack())
        attitude.push_back("wont_attack");
    if (!attitude.empty())
    {
        inf.body << "; " << comma_separated_line(attitude.begin(),
                                                 attitude.end(),
                                                 "; ", "; ");
    }

    const monster_spells &hspell_pass = mons.spells;
    bool found_spell = false;

    for (unsigned int i = 0; i < hspell_pass.size(); ++i)
    {
        if (!found_spell)
        {
            inf.body << "\n\nMonster Spells:\n";
            found_spell = true;
        }

        inf.body << "    " << i << ": "
                 << spell_title(hspell_pass[i].spell)
                 << " (";
        if (hspell_pass[i].flags & MON_SPELL_EMERGENCY)
            inf.body << "emergency, ";
        if (hspell_pass[i].flags & MON_SPELL_NATURAL)
            inf.body << "natural, ";
        if (hspell_pass[i].flags & MON_SPELL_DEMONIC)
            inf.body << "demonic, ";
        if (hspell_pass[i].flags & MON_SPELL_MAGICAL)
            inf.body << "magical, ";
        if (hspell_pass[i].flags & MON_SPELL_WIZARD)
            inf.body << "wizard, ";
        if (hspell_pass[i].flags & MON_SPELL_PRIEST)
            inf.body << "priest, ";
        if (hspell_pass[i].flags & MON_SPELL_BREATH)
            inf.body << "breath, ";
        inf.body << (int) hspell_pass[i].freq << ")";
    }

    bool has_item = false;
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        if (mons.inv[i] != NON_ITEM)
        {
            if (!has_item)
            {
                inf.body << "\n\nMonster Inventory:\n";
                has_item = true;
            }
            inf.body << "    " << i << ": "
                     << mitm[mons.inv[i]].name(DESC_A, false, true);
        }
    }

    if (mons.props.exists("blame"))
    {
        inf.body << "\n\nMonster blame chain:\n";

        const CrawlVector& blame = mons.props["blame"].get_vector();

        for (CrawlVector::const_iterator it = blame.begin();
             it != blame.end(); ++it)
        {
            inf.body << "    " << it->get_string() << "\n";
        }
    }
#endif
}

int describe_monsters(const monster_info &mi, bool force_seen,
                      const string &footer)
{
    describe_info inf;
    bool has_stat_desc = false;
    get_monster_db_desc(mi, inf, has_stat_desc, force_seen);

    bool show_quote = false;

    if (!footer.empty())
    {
        if (inf.footer.empty())
            inf.footer = footer;
        else
            inf.footer += "\n" + footer;
    }

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_monster");
#endif

    int key;
    do
    {
        if (show_quote)
            _print_quote(inf);
        else
        {
            print_description(inf);

            // TODO enne - this should really move into get_monster_db_desc
            // and an additional tutorial string added to describe_info.
            if (crawl_state.game_is_hints())
                hints_describe_monster(mi, has_stat_desc);
        }

        show_quote = !show_quote;
    }
    while (_print_toggle_message(inf, key));

    return key;
}

static const char* xl_rank_names[] =
{
    "weakling",
    "average",
    "experienced",
    "powerful",
    "mighty",
    "great",
    "awesomely powerful",
    "legendary"
};

static string _xl_rank_name(const int xl_rank)
{
    const string rank = xl_rank_names[xl_rank];

    return article_a(rank);
}

string short_ghost_description(const monster *mon, bool abbrev)
{
    ASSERT(mons_is_pghost(mon->type));

    const ghost_demon &ghost = *(mon->ghost);
    const char* rank = xl_rank_names[ghost_level_to_rank(ghost.xl)];

    string desc = make_stringf("%s %s %s", rank,
                               species_name(ghost.species).c_str(),
                               get_job_name(ghost.job));

    if (abbrev || strwidth(desc) > 40)
    {
        desc = make_stringf("%s %s%s",
                            rank,
                            get_species_abbrev(ghost.species),
                            get_job_abbrev(ghost.job));
    }

    return desc;
}

// Describes the current ghost's previous owner. The caller must
// prepend "The apparition of" or whatever and append any trailing
// punctuation that's wanted.
string get_ghost_description(const monster_info &mi, bool concise)
{
    ostringstream gstr;

    const species_type gspecies = mi.u.ghost.species;

    // We're fudging stats so that unarmed combat gets based off
    // of the ghost's species, not the player's stats... exact
    // stats aren't required anyway, all that matters is whether
    // dex >= str. -- bwr
    const int dex = 10;
    int str = 5;

    switch (gspecies)
    {
    case SP_MOUNTAIN_DWARF:
    case SP_DEEP_DWARF:
    case SP_TROLL:
    case SP_OGRE:
    case SP_MINOTAUR:
    case SP_HILL_ORC:
#if TAG_MAJOR_VERSION == 34
    case SP_LAVA_ORC:
#endif
    case SP_CENTAUR:
    case SP_NAGA:
    case SP_MUMMY:
    case SP_GHOUL:
    case SP_FORMICID:
    case SP_VINE_STALKER:
        str += 10;
        break;

    case SP_HUMAN:
    case SP_DEMIGOD:
    case SP_DEMONSPAWN:
        str += 5;
        break;

    default:
        break;
    }

    gstr << mi.mname << " the "
         << skill_title_by_rank(mi.u.ghost.best_skill,
                        mi.u.ghost.best_skill_rank,
                        gspecies,
                        str, dex, mi.u.ghost.religion)
         << ", " << _xl_rank_name(mi.u.ghost.xl_rank) << " ";

    if (concise)
    {
        gstr << get_species_abbrev(gspecies)
             << get_job_abbrev(mi.u.ghost.job);
    }
    else
    {
        gstr << species_name(gspecies)
             << " "
             << get_job_name(mi.u.ghost.job);
    }

    if (mi.u.ghost.religion != GOD_NO_GOD)
    {
        gstr << " of "
             << god_name(mi.u.ghost.religion);
    }

    return gstr.str();
}

void describe_skill(skill_type skill)
{
    ostringstream data;

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_skill");
#endif

    data << get_skill_description(skill, true);

    print_description(data.str());
    getchm();
}

#ifdef USE_TILE
string get_command_description(const command_type cmd, bool terse)
{
    string lookup = command_to_name(cmd);

    if (!terse)
        lookup += " verbose";

    string result = getLongDescription(lookup);
    if (result.empty())
    {
        if (!terse)
        {
            // Try for the terse description.
            result = get_command_description(cmd, true);
            if (!result.empty())
                return result + ".";
        }
        return command_to_name(cmd);
    }

    return result.substr(0, result.length() - 1);
}
#endif

void alt_desc_proc::nextline()
{
    ostr << "\n";
}

void alt_desc_proc::print(const string &str)
{
    ostr << str;
}

int alt_desc_proc::count_newlines(const string &str)
{
    int count = 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] == '\n')
            count++;
    }
    return count;
}

void alt_desc_proc::trim(string &str)
{
    int idx = str.size();
    while (--idx >= 0)
    {
        if (str[idx] != '\n')
            break;
    }
    str.resize(idx + 1);
}

bool alt_desc_proc::chop(string &str)
{
    int loc = -1;
    for (size_t i = 1; i < str.size(); i++)
        if (str[i] == '\n' && str[i-1] == '\n')
            loc = i;

    if (loc == -1)
        return false;

    str.resize(loc);
    return true;
}

void alt_desc_proc::get_string(string &str)
{
    str = replace_all(ostr.str(), "\n\n\n\n", "\n\n");
    str = replace_all(str, "\n\n\n", "\n\n");

    trim(str);
    while (count_newlines(str) > h)
    {
        if (!chop(str))
            break;
    }
}

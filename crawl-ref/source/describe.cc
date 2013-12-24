/**
 * @file
 * @brief Functions used to print information about various game objects.
**/

#include "AppHdr.h"

#include "describe.h"
#include "process_desc.h"
#include "database.h"

#include <stdio.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <numeric>

#include "externs.h"
#include "options.h"
#include "species.h"

#include "ability.h"
#include "artefact.h"
#include "cio.h"
#include "clua.h"
#include "command.h"
#include "decks.h"
#include "delay.h"
#include "directn.h"
#include "food.h"
#include "ghost.h"
#include "goditem.h"
#include "godpassive.h"
#include "invent.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "evoke.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "mon-chimera.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "output.h"
#include "player.h"
#include "quiver.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-summoning.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stash.h"
#include "terrain.h"
#include "transform.h"
#include "hints.h"
#include "xom.h"
#include "mon-info.h"
#ifdef USE_TILE_LOCAL
#include "tilereg-crt.h"
#endif
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
    textcolor(LIGHTGREY);

    default_desc_proc proc;
    process_description<default_desc_proc>(proc, inf);
}

static void _print_quote(const describe_info &inf)
{
    clrscr();
    textcolor(LIGHTGREY);

    default_desc_proc proc;
    process_quote<default_desc_proc>(proc, inf);
}

static const char* _jewellery_base_ability_string(int subtype)
{
    switch (subtype)
    {
    case RING_REGENERATION:      return "Regen";
    case RING_SUSTAIN_ABILITIES: return "SustAb";
    case RING_SUSTENANCE:        return "Hunger-";
    case RING_WIZARDRY:          return "Wiz";
    case RING_FIRE:              return "Fire";
    case RING_ICE:               return "Ice";
    case RING_TELEPORTATION:     return "+/*Tele";
    case RING_TELEPORT_CONTROL:  return "+cTele";
    case AMU_CLARITY:            return "Clar";
    case AMU_WARDING:            return "Ward";
    case AMU_RESIST_CORROSION:   return "rCorr";
    case AMU_THE_GOURMAND:       return "Gourm";
    case AMU_CONSERVATION:       return "Cons";
#if TAG_MAJOR_VERSION == 34
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
        { "Hunger", ARTP_METABOLISM,            2 }, // handled specially
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
        { "MR",     ARTP_MAGIC,                 2 }, // handled specially

        // Quantitative attributes
        { "HP",     ARTP_HP,                    0 },
        { "MP",     ARTP_MAGICAL_POWER,         0 },
        { "AC",     ARTP_AC,                    0 },
        { "EV",     ARTP_EVASION,               0 },
        { "Str",    ARTP_STRENGTH,              0 },
        { "Dex",    ARTP_DEXTERITY,             0 },
        { "Int",    ARTP_INTELLIGENCE,          0 },
        { "Acc",    ARTP_ACCURACY,              0 },
        { "Dam",    ARTP_DAMAGE,                0 },

        // Qualitative attributes
        { "SInv",   ARTP_EYESIGHT,              2 },
        { "Stlth",  ARTP_STEALTH,               2 }, // handled specially
        { "Curse",  ARTP_CURSED,                2 },
        { "Clar",   ARTP_CLARITY,               2 },
        { "RMsl",   ARTP_RMSL,                  2 },
        { "Regen",  ARTP_REGENERATION,          2 },
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
                const int sval = min(abs(val), 3);
                work << propanns[i].name
                     << string(sval, (val > 0 ? '+' : '-'));
                break;
            }
            case 2: // e.g. rPois or SInv
                if (propanns[i].prop == ARTP_CURSED && val < 1)
                    continue;

                work << propanns[i].name;

                // these need special handling, so we don't give anything away
                if (propanns[i].prop == ARTP_METABOLISM && val > 2)
                    work << "+";
                else if (propanns[i].prop == ARTP_METABOLISM && val < 0)
                    work << "-";
                else if (propanns[i].prop == ARTP_STEALTH
                         || propanns[i].prop == ARTP_MAGIC)
                {
                    if (val > 50)
                        work << "++";
                    else if (val > 20)
                        work << "+";
                    else if (val < -50)
                        work << "--";
                    else if (val < 0)
                        work << "-";
                }
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
    artefact_desc_properties(item, proprt, known);

    const property_descriptor propdescs[] =
    {
        { ARTP_AC, "It affects your AC (%d).", false },
        { ARTP_EVASION, "It affects your evasion (%d).", false},
        { ARTP_STRENGTH, "It affects your strength (%d).", false},
        { ARTP_INTELLIGENCE, "It affects your intelligence (%d).", false},
        { ARTP_DEXTERITY, "It affects your dexterity (%d).", false},
        { ARTP_ACCURACY, "It affects your accuracy (%d).", false},
        { ARTP_DAMAGE, "It affects your damage-dealing abilities (%d).", false},
        { ARTP_FIRE, "fire", true},
        { ARTP_COLD, "cold", true},
        { ARTP_ELECTRICITY, "It insulates you from electricity.", false},
        { ARTP_POISON, "It protects you from poison.", false},
        { ARTP_NEGATIVE_ENERGY, "negative energy", true},
        { ARTP_MAGIC, "It affects your resistance to hostile enchantments.", false},
        { ARTP_HP, "It affects your health (%d).", false},
        { ARTP_MAGICAL_POWER, "It affects your magic capacity (%d).", false},
        { ARTP_EYESIGHT, "It enhances your eyesight.", false},
        { ARTP_INVISIBLE, "It lets you turn invisible.", false},
        { ARTP_FLY, "It lets you fly.", false},
        { ARTP_BLINK, "It lets you blink.", false},
        { ARTP_BERSERK, "It lets you go berserk.", false},
        { ARTP_NOISES, "It makes noises.", false},
        { ARTP_PREVENT_SPELLCASTING, "It prevents spellcasting.", false},
        { ARTP_CAUSE_TELEPORTATION, "It causes teleportation.", false},
        { ARTP_PREVENT_TELEPORTATION, "It prevents most forms of teleportation.",
          false},
        { ARTP_ANGRY,  "It makes you angry.", false},
        { ARTP_CURSED, "It may recurse itself.", false},
        { ARTP_CLARITY, "It protects you against confusion.", false},
        { ARTP_MUTAGENIC, "It causes magical contamination when unequipped.", false},
        { ARTP_RMSL, "It protects you from missiles.", false},
        { ARTP_FOG, "It can be evoked to emit clouds of fog.", false},
        { ARTP_REGENERATION, "It increases your rate of regeneration.", false},
    };

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

    // Some special cases which don't fit into the above.
    if (known_proprt(ARTP_METABOLISM))
    {
        if (proprt[ARTP_METABOLISM] >= 3)
            description += "\nIt greatly speeds your metabolism.";
        else if (proprt[ARTP_METABOLISM] >= 1)
            description += "\nIt speeds your metabolism. ";
        if (proprt[ARTP_METABOLISM] <= -3)
            description += "\nIt greatly slows your metabolism.";
        else if (proprt[ARTP_METABOLISM] <= -1)
            description += "\nIt slows your metabolism. ";
    }

    if (known_proprt(ARTP_STEALTH))
    {
        const int stval = proprt[ARTP_STEALTH];
        char buf[80];
        snprintf(buf, sizeof buf, "\nIt makes you %s%s stealthy.",
                 (stval < -20 || stval > 20) ? "much " : "",
                 (stval < 0) ? "less" : "more");
        description += buf;
    }

    return description;
}
#undef known_proprt

static const char *trap_names[] =
{
    "dart", "arrow", "spear",
    "teleport", "alarm", "blade",
    "bolt", "net", "Zot", "needle",
    "shaft", "passage", "pressure plate", "web",
#if TAG_MAJOR_VERSION == 34
    "gas",
#endif
};

string trap_name(trap_type trap)
{
    COMPILE_CHECK(ARRAYSZ(trap_names) == NUM_TRAPS);

    if (trap >= TRAP_DART && trap < NUM_TRAPS)
        return trap_names[trap];
    return "";
}

int str_to_trap(const string &s)
{
    // "Zot trap" is capitalised in trap_names[], but the other trap
    // names aren't.
    const string tspec = lowercase_string(s);

    // allow a couple of synonyms
    if (tspec == "random" || tspec == "any")
        return TRAP_RANDOM;
    else if (tspec == "nonteleport" || tspec == "noteleport"
             || tspec == "nontele" || tspec == "notele")
    {
        return TRAP_NONTELEPORT;
    }

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
        " who hovers in mid-air",
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
        " who is covered in oozing lacerations",
        " and the head of a frog",
        " and the head of a reindeer",
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
                            << (player_mutation_level(MUT_SAPROVOROUS) == 3 ?
                                " - yum!" : ".");
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
    description += "\nAccuracy rating: ";
    _append_value(description, property(item, PWPN_HIT), true);
    description += "    ";

    description += "Damage rating: ";
    _append_value(description, property(item, PWPN_DAMAGE), false);
    description += "   ";

    description += "Base attack delay: ";
    _append_value(description, property(item, PWPN_SPEED) * 10, false);
   description += "%";
}

static string _corrosion_resistance_string(const item_def &item)
{
    const int ench = item.base_type == OBJ_WEAPONS ? item.plus2 : item.plus;
    const char* format = "\nIts enchantment level renders it %s to acidic "
                         "corrosion.";

    if (is_artefact(item))
        return "";
    if (ench >= 5 && item_ident(item, ISFLAG_KNOW_PLUSES))
        return make_stringf(format, "immune");
    else if (ench >= 4 && item_ident(item, ISFLAG_KNOW_PLUSES))
        return make_stringf(format, "extremely resistant");
    else if (item.base_type == OBJ_ARMOUR
             && item.sub_type == ARM_CRYSTAL_PLATE_ARMOUR)
    {
        return "\nBeing made of crystal renders it very resistant to acidic "
               "corrosion.";
    }
    else if (get_equip_race(item) == ISFLAG_DWARVEN)
    {
        return "\nBeing of dwarven fabrication renders it very resistant to "
               "acidic corrosion.";
    }
    else if (ench >= 3 && item_ident(item, ISFLAG_KNOW_PLUSES))
        return make_stringf(format, "resistant");
    else if (ench >= 2 && item_ident(item, ISFLAG_KNOW_PLUSES))
        return make_stringf(format, "somewhat resistant");
    else
        return "";
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
        if (you.species == SP_FORMICID)
            description += "It is a weapon for two hand-pairs.";
        else
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
        switch (weapon_skill(item))
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
            description += "It emits flame when wielded, causing extra "
                "injury to most foes and up to double damage against "
                "particularly susceptible opponents.";
            if (damtype == DVORP_SLICING || damtype == DVORP_CHOPPING)
            {
                description += " Big, fiery blades are also staple armaments "
                    "of hydra-hunters.";
            }
            break;
        case SPWPN_FREEZING:
            description += "It has been specially enchanted to freeze "
                "those struck by it, causing extra injury to most foes "
                "and up to double damage against particularly "
                "susceptible opponents. It can also slow down "
                "cold-blooded creatures.";
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
        case SPWPN_DRAGON_SLAYING:
            description += "This legendary weapon is deadly to all "
                "dragonkind.";
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
            description += "Attacks with this weapon are significantly faster, "
                "but cause slightly less damage.";
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
        case SPWPN_VAMPIRICISM:
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

    const bool launcher = is_range_weapon(item);
    if (verbose)
    {
        description += "\n\nThis weapon falls into the";

        const skill_type skill =
            is_range_weapon(item)? range_skill(item) : weapon_skill(item);

        description +=
            make_stringf(" '%s' category. ",
                         skill == SK_FIGHTING ? "buggy" : skill_name(skill));

        if (!launcher)
            description += _handedness_string(item);

        if (!you.could_wield(item, true))
            description += "\nIt is too large for you to wield.";
    }

    if (verbose)
    {
        if (is_demonic(item) && !launcher)
            description += "\nDemonspawn deal slightly more damage with it.";
        else if (get_equip_race(item) != ISFLAG_NO_RACE)
        {
            iflags_t race = get_equip_race(item);

            if (race == ISFLAG_DWARVEN)
            {
                description += "\nIt is well-crafted and durable. Dwarves "
                               "deal slightly more damage with it.";
            }

            if (race == ISFLAG_ORCISH)
                description += "\nOrcs deal slightly more damage with it.";

            if (race == ISFLAG_ELVEN)
                description += "\nElves are slightly more accurate with it.";
        }
    }

    if (!is_artefact(item))
    {
        if (item_ident(item, ISFLAG_KNOW_PLUSES)
            && item.plus >= MAX_WPN_ENCHANT && item.plus2 >= MAX_WPN_ENCHANT)
        {
            description += "\nIt cannot be enchanted further.";
        }
        else
        {
            description += "\nIt can be maximally enchanted to +";
            _append_value(description, MAX_WPN_ENCHANT, false);
            if (item.sub_type != WPN_BLOWGUN)
            {
                description += ", +";
                _append_value(description, MAX_WPN_ENCHANT, false);
            }
            description += ".";
        }
    }

    description += _corrosion_resistance_string(item);

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

    if (item.sub_type == MI_THROWING_NET)
    {
        if (item.plus > 1 || item.plus < 0)
        {
            string how;

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

            description += "\n\nIt looks ";
            description += how;
            description += ".";
        }
    }

    const bool can_launch = has_launcher(item);
    const bool can_throw  = is_throwable(&you, item, true);
    bool always_destroyed = false;

    if (item.special && item_type_known(item))
    {
        description += "\n\n";
        string bolt_name;

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
            break;
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
            always_destroyed = true;
            break;
        case SPMSL_EXPLODING:
            description += "It will explode into fragments upon "
                "hitting a target, hitting an obstruction, or reaching "
                "the end of its range.";
            always_destroyed = true;
            break;
        case SPMSL_STEEL:
            description += "Compared to normal ammo, it does 30% more "
                "damage, is destroyed upon impact only 1/10th of the "
                "time, and weighs three times as much.";
            break;
        case SPMSL_SILVER:
            description += "Silver sears all those touched by chaos. "
                "Compared to normal ammo, it does 75% more damage to "
                "chaotic and magically transformed beings. It also does "
                "extra damage against mutated beings according to how "
                "mutated they are. With due care, silver ammo can still "
                "be handled by those it affects. It weighs twice as much "
                "as normal ammo.";
            break;
        }
    }

    if (always_destroyed)
        description += "\nIt will always be destroyed upon impact.";
    else if (item.sub_type != MI_THROWING_NET)
        append_missile_info(description);

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

void append_missile_info(string &description)
{
    description += "\nAll pieces of ammunition may get destroyed upon impact.";
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
            description += "It is very cumbersome, thus slowing your movement.";
            break;
        case SPARM_FLYING:
            description += "It can be activated to allow its wearer to "
                "fly indefinitely.";
            break;
        case SPARM_JUMPING:
            description += "It can be activated to allow its wearer to "
                "perform a jumping attack.";
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

        case SPARM_PRESERVATION:
            description += "It protects its wearer's possessions "
                "from damage and destruction.";
            break;

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
            description += "These improve your skills with ranged weaponry "
                "but interfere slightly with melee combat.";
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
    else if (get_equip_race(item) != ISFLAG_NO_RACE)
    {
        // Randart armour can't be racial.
        description += "\n";

        iflags_t race = get_equip_race(item);

        if (race == ISFLAG_DWARVEN)
            description += "\nIt is well-crafted and durable.";
        else if (race == ISFLAG_ELVEN)
        {
            if (get_item_slot(item) == EQ_BODY_ARMOUR)
            description += "\nIt is well-crafted and unobstructive";
            else
                description += "\nIt is well-crafted and lightweight";
            if (item.sub_type == ARM_CLOAK || item.sub_type == ARM_BOOTS)
                description += ", and helps its wearer avoid being noticed";
            description += ".";
        }

        description += "\nIt fits ";
        description += (race == ISFLAG_DWARVEN) ? "dwarves" :
                       (race == ISFLAG_ELVEN)   ? "elves"
                                                : "orcs";
        description += " well.";
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

    description += _corrosion_resistance_string(item);

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

    if ((verbose || is_artefact(item))
        && item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        // Explicit description of ring power (useful for randarts)
        // Note that for randarts we'll print out the pluses even
        // in the case that its zero, just to avoid confusion. -- bwr
        if (item.plus != 0
            || item.sub_type == RING_SLAYING && item.plus2 != 0
            || is_artefact(item))
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
                if (item.plus != 0)
                {
                    description += "\nIt affects your accuracy (";
                    _append_value(description, item.plus, true);
                    description += ").";
                }

                if (item.plus2 != 0)
                {
                    description += "\nIt affects your damage-dealing abilities (";
                    _append_value(description, item.plus2, true);
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

    // Artefact properties.
    if (is_artefact(item))
    {
        string rand_desc = _randart_descrip(item);
        if (!rand_desc.empty())
            description += rand_desc;
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

bool is_dumpable_artefact(const item_def &item, bool verbose)
{
    if (is_known_artefact(item))
        return item_ident(item, ISFLAG_KNOW_PROPERTIES);
    else if (!verbose || !item_type_known(item))
        return false;
    else if (item.base_type == OBJ_ARMOUR)
    {
        const int spec_ench = get_armour_ego_type(item);
        return spec_ench >= SPARM_RUNNING && spec_ench <= SPARM_PRESERVATION;
    }
    else if (item.base_type == OBJ_JEWELLERY)
        return true;

    return false;
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
                    << " colour: " << static_cast<int>(item.colour)
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
                if (item.base_type == OBJ_JEWELLERY)
                    description << "\n";
                else
                    need_base_desc = false;

                const string quote = getQuoteString(get_artefact_name(item));
                if (!quote.empty())
                    description << "\n" << quote;
            }
        }

        if (need_base_desc)
        {
            string db_name = item.name(DESC_DBNAME, true, false, false);
            string db_desc = getLongDescription(db_name);
            if (!noquote && !is_known_artefact(item))
            {
                const unsigned int lineWidth = get_number_of_cols();
                const          int height    = get_number_of_lines();

                string quote = getQuoteString(db_name);

                if (count_desc_lines(db_desc, lineWidth)
                    + count_desc_lines(quote, lineWidth) <= height)
                {
                    if (!db_desc.empty() && !quote.empty())
                        db_desc += "\n";
                    db_desc += quote;
                }
            }

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
        if (item_type_known(item))
        {
            const int max_charges = wand_max_charges(item.sub_type);
            if (item.plus < max_charges
                || !item_ident(item, ISFLAG_KNOW_PLUSES))
            {
                description << "\nIt can have at most " << max_charges
                            << " charges.";
            }
            else
                description << "\nIt is fully charged.";
        }

        if (item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus == 0
            || item.plus2 == ZAPCOUNT_EMPTY)
        {
            description << "\nUnfortunately, it has no charges left.";
        }
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

            switch (mons_corpse_effect(item.mon_type))
            {
            case CE_POISONOUS:
                description << "\n\nThis meat is poisonous.";
                break;
            case CE_MUTAGEN:
                if (you.species != SP_GHOUL)
                {
                    description << "\n\nEating this meat will cause random "
                                   "mutations.";
                }
                break;
            case CE_ROT:
                if (you.species != SP_GHOUL)
                    description << "\n\nEating this meat will cause rotting.";
                break;
            case CE_CONTAMINATED:
                if (player_mutation_level(MUT_SAPROVOROUS) < 3)
                {
                    description << "\n\nMeat like this tastes awful and "
                                   "provides far less nutrition.";
                }
                break;
            case CE_POISON_CONTAM:
                description << "\n\nThis meat is poisonous";
                if (player_mutation_level(MUT_SAPROVOROUS) < 3)
                {
                    description << " and provides less nutrition even for the "
                                   "poison-resistant";
                }
                description << ".";
                break;
            default:
                break;
            }

            if ((god_hates_cannibalism(you.religion)
                   && is_player_same_genus(item.mon_type))
                || (you_worship(GOD_ZIN)
                   && mons_class_intel(item.mon_type) >= I_NORMAL)
                || (is_good_god(you.religion)
                   && mons_class_holiness(item.mon_type) == MH_HOLY))
            {
                description << "\n\n" << uppercase_first(god_name(you.religion))
                            << " disapproves of eating such meat.";
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
                const int num_charges = item.plus2 / ROD_CHARGE_MULT;
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
        if (is_elemental_evoker(item))
        {
            description << "\nOnce released, the spirits within this device "
                           "will dissipate, leaving it inert, though new ones "
                           "may be attracted as its bearer battles through the "
                           "dungeon and grows in power and wisdom.";

            if (!evoker_is_charged(item))
                description << "\n\nThe device is presently inert.";
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
            description << "\nIt";
            if (item_known_cursed(item))
                description << " has a curse placed upon it, and it";

            const int mass = item_mass(item);
            description << " weighs around " << (mass / 10)
                        << "." << (mass % 10)
                        << " aum. "; // arbitrary unit of mass

            if (is_artefact(item))
            {
                if (item.base_type == OBJ_ARMOUR
                    || item.base_type == OBJ_WEAPONS)
                {
                    description << "\n\nThis ancient artefact cannot be changed "
                        "by magic or mundane means.";
                }
                else
                    description << "\n\nIt is an ancient artefact.";
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
    inf.body << get_item_description(item, verbose, false,
                                     crawl_state.game_is_hints_tutorial());
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
    spellbook_contents(dup, item.base_type == OBJ_BOOKS ? RBOOK_READ_SPELL
                                                        : RBOOK_USE_ROD,
                       &fs);
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
        mesclr();
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
        textcolor(LIGHTGREY);

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
        if (can_ingest(item, true, false))
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
        if (you.is_undead != US_UNDEAD) // mummies and lich form only
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
                       + " creatures summoned by this spell.\n";
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
    if (spell_is_useless(spell))
        description += "This spell will have no effect right now.\n";

    _append_spell_stats(spell, description, rod);

    if (crawl_state.player_is_dead())
        return BOOK_NEITHER;

    const string quote = getQuoteString(string(spell_title(spell)) + " spell");
    if (!quote.empty())
        description += "\n" + quote;

    bool form = false;
    if (you_cannot_memorise(spell, form))
        description += "\n" + desc_cannot_memorise_reason(form) + "\n";

    if (item && item->base_type == OBJ_BOOKS && in_inventory(*item)
        && you.form != TRAN_WISP)
    {
        if (you.has_spell(spell))
        {
            description += "\n(F)orget this spell by destroying the book.\n";
            if (you_worship(GOD_SIF_MUNA))
                description +="Sif Muna frowns upon the destroying of books.\n";
            return BOOK_FORGET;
        }
        else if (player_can_memorise_from_spellbook(*item)
                 && !you_cannot_memorise(spell, form))
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

static string _describe_draconian_role(monster_type type)
{
    switch (type)
    {
    case MONS_DRACONIAN_SHIFTER:
        return "It darts around disconcertingly without taking a step.";
    case MONS_DRACONIAN_SCORCHER:
        return "Its scales are sooty from years of magical pyrotechnics.";
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
        return "";
    }
}

static string _describe_draconian_colour(int species)
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
    case MONS_GREY_DRACONIAN:
        return "Its scales and tail are adapted to the water.";
    case MONS_PALE_DRACONIAN:
        return "It is cloaked in a pall of superheated steam.";
    }
    return "";
}

static string _describe_draconian(const monster_info& mi)
{
    string description;
    const int subsp = mi.draco_subspecies();

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
    case MONS_GREY_DRACONIAN:       description += "grey-";    break;
    case MONS_PALE_DRACONIAN:       description += "pale-";    break;
    default:
        break;
    }

    description += "scaled humanoid with wings.";

    if (subsp != MONS_DRACONIAN)
    {
        const string drac_col = _describe_draconian_colour(subsp);
        if (!drac_col.empty())
            description += " " + drac_col;
    }

    if (subsp != mi.type)
    {
        const string drac_role = _describe_draconian_role(mi.type);
        if (!drac_role.empty())
            description += " " + drac_role;
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

// Is the spell worth listing for a monster?
static bool _interesting_mons_spell(spell_type spell)
{
    return spell != SPELL_NO_SPELL && spell != SPELL_MELEE;
}

static string _monster_spells_description(const monster_info& mi)
{
    // Show monster spells and spell-like abilities.
    if (!mons_class_flag(mi.type, M_SPELLCASTER))
        return "";

    const bool caster = mons_class_flag(mi.type, M_ACTUAL_SPELLS);
    const bool priest = mons_class_flag(mi.type, M_PRIEST);
    const bool natural = mons_class_flag(mi.type, M_FAKE_SPELLS);
    string adj = priest ? "divine" : natural ? "special" : "magical";

    const monsterentry *m = get_monster_data(mi.type);
    mon_spellbook_type book = (m->sec);
    const vector<mon_spellbook_type> books = mons_spellbook_list(mi.type);
    const size_t num_books = books.size();

    // If there are really really no spells, print nothing.  Ghosts,
    // player illusions, and random pan lords don't have consistent
    // spell lists, and might not even have spells; omit them as well.
    if (num_books == 0 || books[0] == MST_NO_SPELLS || book == MST_GHOST)
        return "";

    ostringstream result;
    result << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE));

    // The combination of M_ACTUAL_SPELLS with MST_NO_SPELLS means
    // multiple spellbooks.  cjo: the division here gets really
    // arbitrary. For example, wretched stars cast mystic blast, but
    // are not flagged with M_ACTUAL_SPELLS.  Possibly these should be
    // combined.
    if (caster && book == MST_NO_SPELLS)
        result << " has mastered one of the following spellbooks:\n";
    else if (caster)
        result << " has mastered the following spells: ";
    else if (book != MST_NO_SPELLS)
        result << " possesses the following " << adj << " abilities: ";
    else
    {
        result << " possesses one of the following sets of " << adj
               << " abilities: \n";
    }

    // Loop through books and display spells/abilities for each of them
    for (size_t i = 0; i < num_books; ++i)
    {
        // Create spell list containing no duplicate/irrelevant entries
        book = books[i];
        vector<spell_type> book_spells;
        for (int j = 0; j < NUM_MONSTER_SPELL_SLOTS; ++j)
        {
            const spell_type spell = mspell_list[book].spells[j];
            bool match = false;
            for (unsigned int k = 0; k < book_spells.size(); ++k)
                if (book_spells[k] == spell)
                    match = true;

            if (!match && _interesting_mons_spell(spell))
                book_spells.push_back(spell);
        }

        // Display spells for this book
        if (num_books > 1)
            result << (caster ? " Book " : " Set ") << i+1 << ": ";

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

// Describe a monster's (intrinsic) resistances, speed and a few other
// attributes.
static string _monster_stat_description(const monster_info& mi)
{
    ostringstream result;

    // Don't leak or duplicate resistance information for ghost demon
    // monsters, except for (very) ugly things.
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

    if (mons_is_statue(mi.type, true))
    {
        result << uppercase_first(pronoun) << " is very brittle "
               << "and susceptible to disintegration.\n";
    }

    // Is monster susceptible to anything? (On a new line.)
    if (!suscept.empty())
    {
        result << uppercase_first(pronoun) << " is susceptible to "
               << comma_separated_line(suscept.begin(), suscept.end())
               << ".\n";
    }

    const int mr = mi.res_magic();
    // How resistant is it? Same scale as the player.
    if (mr >= 10)
    {
        result << uppercase_first(pronoun)
               << make_stringf(" is %s to hostile enchantments.\n",
                               magic_res_adjective(mr).c_str());
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

    // Seeing/sensing invisible.
    if (mons_class_flag(mi.type, M_SEE_INVIS))
        result << uppercase_first(pronoun) << " can see invisible.\n";
    else if (mons_class_flag(mi.type, M_SENSE_INVIS))
        result << uppercase_first(pronoun) << " can sense the presence of invisible creatures.\n";

    // Unusual monster speed.
    const int speed = mi.base_speed();
    if (speed != 10 && speed != 0)
    {
        result << uppercase_first(pronoun) << " is ";
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
        result << ".\n";
    }

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
    const char *sizes[NUM_SIZE_LEVELS] =
    {
        "tiny",
        "very small",
        "small",
        NULL,     // don't display anything for 'medium'
        "large",
        "very large",
        "giant",
        "huge",
    };

    if (mons_is_feat_mimic(mi.type))
    {
        result << uppercase_first(pronoun) << " is as big as "
               << thing_do_grammar(DESC_A, true, false,
                                   feat_type_name(mi.get_mimic_feature()))
               << "\n";
    }
    else if (sizes[mi.body_size()])
        result << uppercase_first(pronoun) << " is " << sizes[mi.body_size()] << ".\n";

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

    // Except for draconians and player ghosts, I have to admit I find the
    // following special descriptions rather pointless. I certainly can't
    // say I like them, though "It has come for your soul!" and
    // "It wants to drink your blood!" have something going for them. (jpeg)
    switch (mi.type)
    {
    case MONS_VAMPIRE:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAMPIRE_MAGE:
        if (you.is_undead == US_ALIVE && mi.attitude == ATT_HOSTILE)
            inf.body << "\nIt wants to drink your blood!\n";
        break;

    case MONS_REAPER:
        if (you.is_undead == US_ALIVE && mi.attitude == ATT_HOSTILE)
            inf.body <<  "\nIt has come for your soul!\n";
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

    if (mi.intel() <= I_INSECT)
        inf.body << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE)) << " is mindless.\n";

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
        inf.body << make_stringf("\nPlaced by map: %s",
                                 mons.originating_map().c_str());

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
        inf.body << "; " << comma_separated_line(attitude.begin(),
                                                 attitude.end(),
                                                 "; ", "; ");

    if (mons.can_use_spells())
    {
        const monster_spells &hspell_pass = mons.spells;
        bool found_spell = false;

        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
        {
            if (hspell_pass[i] != SPELL_NO_SPELL)
            {
                if (!found_spell)
                {
                    inf.body << "\n\nMonster Spells:\n";
                    found_spell = true;
                }

                inf.body << "    " << i << ": "
                         << spell_title(hspell_pass[i])
                         << " (" << static_cast<int>(hspell_pass[i])
                         << ")";
            }
        }
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
    case SP_LAVA_ORC:
    case SP_CENTAUR:
    case SP_NAGA:
    case SP_MUMMY:
    case SP_GHOUL:
    case SP_FORMICID:
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
                                   "You can provide lesser healing for yourself and others.",
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

//---------------------------------------------------------------
//
// describe_god
//
// Describes the player's standing with his deity.
//
//---------------------------------------------------------------

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

    const string godname = god_name(which_god);
    return (you.piety >= piety_breakpoint(5)) ? "A prized avatar of " + godname + ".":
           (you.piety >= piety_breakpoint(4)) ? "A favoured servant of " + godname + ".":
           (you.piety >= piety_breakpoint(3)) ? "A shining star in the eyes of " + godname + "." :
           (you.piety >= piety_breakpoint(2)) ? "A rising star in the eyes of " + godname + "." :
           (you.piety >= piety_breakpoint(1)) ? uppercase_first(godname) + " is most pleased with you." :
           (you.piety >= piety_breakpoint(0)) ? uppercase_first(godname) + " is pleased with you."
                                              : uppercase_first(godname) + " is noncommittal.";
}

static string _religion_help(god_type god)
{
    string result = "";

    switch (god)
    {
    case GOD_ZIN:
        result += "You can pray at an altar to donate your money.";
        if (!player_under_penance()
            && you.piety >= piety_breakpoint(5)
            && !you.one_time_ability_used[god])
        {
            if (!result.empty())
                result += " ";

            result += "You can have all your mutations cured.";
        }
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
        if (!player_under_penance()
            && you.piety >= piety_breakpoint(5)
            && !you.one_time_ability_used[god])
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
                + "with !p, !* or =p to avoid sacrificing them accidentally.";
        break;

    case GOD_LUGONU:
        if (!player_under_penance()
            && you.piety >= piety_breakpoint(5)
            && !you.one_time_ability_used[god])
        {
            result += "You can pray at an altar to have your weapon "
                      "corrupted.";
        }
        break;

    case GOD_KIKUBAAQUDGHA:
        if (!player_under_penance()
            && you.piety >= piety_breakpoint(5)
            && !you.one_time_ability_used[god])
        {
            result += "You can pray at an altar to have your necromancy "
                      "enhanced.";
        }
        break;

    case GOD_BEOGH:
        result += "You can pray to sacrifice all orcish remains on your "
                  "square.";
        break;

    case GOD_NEMELEX_XOBEH:
        result += "You can pray to sacrifice all items on your square. "
                  "Inscribe items with !p, !* or =p to avoid sacrificing "
                  "them accidentally. See the detailed description to "
                  "sacrifice only some kinds of items.";
        break;

    case GOD_FEDHAS:
        if (you.piety >= piety_breakpoint(0))
        {
            result += "Evolving plants requires fruit, and evolving "
                      "fungi requires piety.";
        }

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
    {"Traitor",            "Zealot",                "Exhumer",                  "Fey %s",
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
     "Fiendish",           "Demolition %s",         "Pandemonic",               "Champion of Chaos"},

    // Sif Muna -- scholarly theme.
    {"Ignorant",           "Disciple",              "Student",                  "Adept",
     "Scribe",             "Scholar",               "Sage",                     "Genius of the Arcane"},

    // Trog -- anger theme.
    {"Puny",               "Troglodyte",            "Angry Troglodyte",         "Frenzied",
     "%s of Prey",         "Rampant",               "Wild %s",                  "Bane of Scribes"},

    // Nemelex Xobeh -- alluding to Tarot and cards.
    {"Unlucky %s",         "Pannier",               "Jester",                   "Fortune-Teller",
     "Soothsayer",         "Magus",                 "Cardsharp",                "Hand of Fortune"},

    // Elyvilon.
    {"Sinner",             "Comforter",             "Caregiver",                "Practitioner",
     "Pacifier",           "Purifying %s",          "Faith Healer",             "Bringer of Life"},

    // Lugonu -- distortion theme.
    {"Pure",               "Abyss-Baptised",        "Unweaver",                 "Distorting %s",
     "Agent of Entropy",   "Schismatic",            "Envoy of Void",            "Corrupter of Planes"},

    // Beogh -- messiah theme.
    {"Apostate",           "Messenger",             "Proselytiser",             "Priest",
     "Missionary",         "Evangelist",            "Apostle",                  "Messiah"},

    // Jiyva -- slime and jelly theme.
    {"Scum",               "Jelly",                 "Squelcher",                "Dissolver",
     "Putrid Slime",       "Consuming %s",          "Archjelly",                "Royal Jelly"},

    // Fedhas Madash -- nature theme.
    {"Walking Fertiliser", "Green %s",              "Inciter",                  "Photosynthesist",
     "Cultivator",         "Green Death",           "Nimbus",                   "Force of Nature"},

    // Cheibriados -- slow theme
    {"Unwound %s",         "Timekeeper",            "Righteous Timekeeper",     "Chronographer",
     "Splendid Chronographer", "Chronicler",        "Eternal Chronicler",       "Ticktocktomancer"},

    // Ashenzari -- divination theme
    {"Star-crossed",       "Cursed",                "Initiated",                "Seer",
     "Soothsayer",         "Oracle",                "Illuminatus",              "Omniscient"},
};

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

string god_title(god_type which_god, species_type which_species, int piety)
{
    string title;
    if (player_under_penance(which_god))
        title = divine_title[which_god][0];
    else
        title = divine_title[which_god][_piety_level(piety)];

    title = replace_all(title, "%s",
                        species_name(which_species, true, false));

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

    string broken;
    if (which_god != GOD_NEMELEX_XOBEH)
    {
        broken = get_god_powers(which_god);
        if (!broken.empty())
        {
            linebreak_string(broken, width);
            display_tagged_block(broken);
            cprintf("\n");
            cprintf("\n");
        }
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
        case GOD_TROG:
            broken = "Note that Trog does not demand training of the "
                     "Invocations skill. All abilities are purely based on "
                     "piety.";
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
                broken = "The piety increase when sacrificing mostly depends "
                         "on the value of the item. To prevent items from "
                         "being accidentally sacrificed, you can "
                         "<w>i</w>nscribe them with <w>!p</w> (protects the "
                         "whole stack), with <w>=p</w> (protects only the "
                         "item), or with <w>!D</w> (causes item to be ignored "
                         "in sacrifices)."
                         "\n\n"
                         "Nemelex Xobeh gifts various types of decks of cards. "
                         "Each deck type comes in three power levels: plain, "
                         "ornate, legendary. The latter contain very powerful "
                         "card effects, potentially hazardous. High piety and "
                         "Evocations skill help here, as the power of Nemelex's "
                         "abilities is governed by Evocations instead of "
                         "Invocations. The type of the deck gifts strongly "
                         "depends on the dominating item class sacrificed. "
                         "Press a letter now to toggle a class:\n";

                for (int i = 0; i < NUM_NEMELEX_GIFT_TYPES; ++i)
                {
                    const bool active = you.nemelex_sacrificing[i];
                    string desc = "";
                    switch (i)
                    {
                    case NEM_GIFT_ESCAPE:
                        desc = "decks of Escape      -- armour";
                        break;
                    case NEM_GIFT_DESTRUCTION:
                        desc = "decks of Destruction -- weapons and ammunition";
                        break;
                    case NEM_GIFT_DUNGEONS:
                        desc = "decks of Dungeons    -- jewellery, books, "
                                                    "miscellaneous items";
                        break;
                    case NEM_GIFT_SUMMONING:
                        desc = "decks of Summoning   -- corpses, chunks, blood";
                        break;
                    case NEM_GIFT_WONDERS:
                        desc = "decks of Wonders     -- consumables: food, potions, "
                                                    "scrolls, wands";
                        break;
                    }
                    broken += make_stringf(" <white>%c</white> %s%s%s\n",
                                           'a' + (char) i,
                                           active ? "+ " : "- <darkgrey>",
                                           desc.c_str(),
                                           active ? "" : "</darkgrey>");
                }
            }
        break;
        case GOD_ASHENZARI:
            if (which_god == you.religion && piety_rank() > 1)
                broken = _describe_ash_skill_boost();
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

    const int bottom_line = min(30, get_number_of_lines());

    cgotoxy(1, bottom_line);
    formatted_string::parse_string(
        "Press '<w>!</w>' or '<w>^</w>'"
#ifdef USE_TILE_LOCAL
        " or <w>Right-click</w>"
#endif
        " to toggle between the overview and the detailed "
        "description.").display();

    mouse_control mc(MOUSE_MODE_MORE);

    const int keyin = getchm();
    if (you_worship(GOD_NEMELEX_XOBEH)
        && keyin >= 'a' && keyin < 'a' + (char) NUM_NEMELEX_GIFT_TYPES)
    {
        const int num = keyin - 'a';
        you.nemelex_sacrificing.set(num, !you.nemelex_sacrificing[num]);
        _detailed_god_description(which_god);
    }
    else if (keyin == '!' || keyin == CK_MOUSE_CMD || keyin == '^')
        describe_god(which_god, true);
}

void describe_god(god_type which_god, bool give_title)
{
    int colour;              // Colour used for some messages.

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

    colour = god_colour(which_god);

    // Print long god's name.
    textcolor(colour);
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
        textcolor(colour);

        string title = god_title(which_god, you.species, you.piety);
        cprintf("%s", title.c_str());
    }

    // mv: Now let's print favour as Brent suggested.
    // I know these messages aren't perfect so if you can think up
    // something better, do it.

    textcolor(LIGHTGREY);
    cprintf("\n\nFavour - ");
    textcolor(colour);

    //mv: Player is praying at altar without appropriate religion.
    // It means player isn't checking his own religion and so we only
    // display favour and go out.
    if (!you_worship(which_god))
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

        cprintf((which_god == GOD_NEMELEX_XOBEH
                     && which_god_penance > 0 && which_god_penance <= 100)
                                             ? "%s doesn't play fair with you." :
                 (which_god_penance >= 50)   ? "%s's wrath is upon you!" :
                 (which_god_penance >= 20)   ? "%s is annoyed with you." :
                 (which_god_penance >=  5)   ? "%s well remembers your sins." :
                 (which_god_penance >   0)   ? "%s is ready to forgive your sins." :
                 (you.worshipped[which_god]) ? "%s is ambivalent towards you."
                                             : "%s is neutral towards you.",
                 uppercase_first(god_name(which_god)).c_str());
    }
    else
    {
        cprintf(_describe_favour(which_god).c_str());
        if (which_god == GOD_ASHENZARI)
            cprintf("\n%s", ash_describe_bondage(ETF_ALL, true).c_str());

        //mv: The following code shows abilities given by your god (if any).

        textcolor(LIGHTGREY);
        const char *header = "Granted powers:";
        const char *cost   = "(Cost)";
        cprintf("\n\n%s%*s%s\n", header,
                get_number_of_cols() - 1 - strwidth(header) - strwidth(cost),
                "", cost);
        textcolor(colour);

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
            const char *how = (you.piety >= 150) ? "carefully" :
                              (you.piety >= 100) ? "often" :
                              (you.piety >=  50) ? "sometimes" :
                                                   "occasionally";

            cprintf("%s %s shields you from chaos.\n",
                    uppercase_first(god_name(which_god)).c_str(), how);
        }
        else if (which_god == GOD_SHINING_ONE)
        {
            have_any = true;
            const char *how = (you.piety >= 150) ? "carefully" :
                              (you.piety >= 100) ? "often" :
                              (you.piety >=  50) ? "sometimes" :
                                                   "occasionally";

            cprintf("%s %s shields you from negative energy.\n",
                    uppercase_first(god_name(which_god)).c_str(), how);
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
            if (!player_under_penance())
            {
                have_any = true;
                const char *how = (you.piety >= 150) ? "carefully" :
                                  (you.piety >= 100) ? "often" :
                                  (you.piety >=  50) ? "sometimes" :
                                                       "occasionally";

                cprintf("%s %s shields your consumables from destruction.\n",
                        uppercase_first(god_name(which_god)).c_str(), how);
            }
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
            string buf = "You can pray to bestow "
                         + apostrophise(god_name(which_god))
                         + " curse upon scrolls that usually remove them.";
            _print_final_god_abil_desc(which_god, buf,
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
            cprintf("None.\n");
    }

    const int bottom_line = min(30, get_number_of_lines());

    // Only give this additional information for worshippers.
    if (which_god == you.religion)
    {
        string extra = get_linebreak_string(_religion_help(which_god),
                                            numcols).c_str();
        cgotoxy(1, bottom_line - count(extra.begin(), extra.end(), '\n')-1,
                GOTO_CRT);
        textcolor(LIGHTGREY);
        cprintf("%s", extra.c_str());
    }

    cgotoxy(1, bottom_line);
    textcolor(LIGHTGREY);
    formatted_string::parse_string(
        "Press '<w>!</w>' or '<w>^</w>'"
#ifdef USE_TILE_LOCAL
        " or <w>Right-click</w>"
#endif
        " to toggle between the overview and the detailed "
        "description.").display();

    mouse_control mc(MOUSE_MODE_MORE);
    const int keyin = getchm();
    if (keyin == '!' || keyin == CK_MOUSE_CMD || keyin == '^')
        _detailed_god_description(which_god);
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
            result += "This is the skill all of Nemelex's abilities rely on.";
        }
        break;

    case SK_SPELLCASTING:
        if (you_worship(GOD_TROG))
        {
            result += "\n";
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

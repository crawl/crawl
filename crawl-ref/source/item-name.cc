/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "item-name.h"

#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "decks.h"
#include "describe.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h" // LSTATE_STILL_WINDS
#include "errors.h" // sysfail
#include "evoke.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::want_curses, no_haste
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "level-state-type.h"
#include "libutil.h"
#include "makeitem.h"
#include "notes.h"
#include "options.h"
#include "orb-type.h"
#include "player.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-goditem.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "tag-version.h"
#include "throw.h"
#include "transform.h"
#include "unicode.h"
#include "unwind.h"
#include "viewgeom.h"
#include "zot.h" // gem_clock_active

static bool _is_consonant(char let);
static char _random_vowel();
static char _random_cons();
static string _random_consonant_set(size_t seed);

static void _maybe_identify_pack_item()
{
    for (auto &item : you.inv)
        if (item.defined() && !get_ident_type(item))
            maybe_identify_base_type(item);
}

// quant_name is useful since it prints out a different number of items
// than the item actually contains.
string quant_name(const item_def &item, int quant,
                  description_level_type des, bool terse)
{
    // item_name now requires a "real" item, so we'll mangle a tmp
    item_def tmp = item;
    tmp.quantity = quant;

    return tmp.name(des, terse);
}

static const char* _interesting_origin(const item_def &item)
{
    if (origin_as_god_gift(item) != GOD_NO_GOD)
        return "god gift";

    if (item.orig_monnum == MONS_DONALD && get_equip_desc(item)
        && item.is_type(OBJ_ARMOUR, ARM_KITE_SHIELD))
    {
        return "Donald";
    }

    return nullptr;
}

/**
 * What inscription should be appended to the given item's name?
 */
static string _item_inscription(const item_def &item)
{
    vector<string> insparts;

    if (const char *orig = _interesting_origin(item))
    {
        if (bool(Options.show_god_gift)
            || Options.show_god_gift == maybe_bool::maybe
                && !fully_identified(item))
        {
            insparts.push_back(orig);
        }
    }

    if (is_artefact(item) && item_ident(item, ISFLAG_KNOW_PROPERTIES))
    {
        const string part = artefact_inscription(item);
        if (!part.empty())
            insparts.push_back(part);
    }

    if (!item.inscription.empty())
        insparts.push_back(item.inscription);

    if (insparts.empty())
        return "";

    return make_stringf(" {%s}",
                        comma_separated_line(begin(insparts),
                                             end(insparts),
                                             ", ").c_str());
}

string item_def::name(description_level_type descrip, bool terse, bool ident,
                      bool with_inscription, bool quantity_in_words,
                      iflags_t ignore_flags) const
{
    if (crawl_state.game_is_arena())
        ignore_flags |= ISFLAG_KNOW_PLUSES | ISFLAG_COSMETIC_MASK;

    if (descrip == DESC_NONE)
        return "";

    ostringstream buff;

    const string auxname = name_aux(descrip, terse, ident, with_inscription,
                                    ignore_flags);

    const bool startvowel     = is_vowel(auxname[0]);
    const bool qualname       = (descrip == DESC_QUALNAME);

    if (descrip == DESC_INVENTORY_EQUIP || descrip == DESC_INVENTORY)
    {
        if (in_inventory(*this)) // actually in inventory
        {
            buff << index_to_letter(link);
            if (terse)
                buff << ") ";
            else
                buff << " - ";
        }
        else
            descrip = DESC_A;
    }

    // XXX EVIL HACK: randbooks are always ID'd..?
    if (base_type == OBJ_BOOKS && is_random_artefact(*this))
        ident = true;

    if (base_type == OBJ_BOOKS && book_has_title(*this, ident))
    {
        if (descrip != DESC_DBNAME)
            descrip = DESC_PLAIN;
    }

    if (terse && descrip != DESC_DBNAME)
        descrip = DESC_PLAIN;

    monster_flags_t corpse_flags;

    // no "a dragon scales"
    const bool always_plural = armour_is_hide(*this)
                               && sub_type != ARM_TROLL_LEATHER_ARMOUR;

    if ((base_type == OBJ_CORPSES && is_named_corpse(*this)
         && !(((corpse_flags.flags = props[CORPSE_NAME_TYPE_KEY].get_int64())
               & MF_NAME_SPECIES)
              && !(corpse_flags & MF_NAME_DEFINITE))
         && !(corpse_flags & MF_NAME_SUFFIX)
         && !starts_with(get_corpse_name(*this), "shaped "))
        || item_is_orb(*this)
        || item_is_horn_of_geryon(*this)
        || (ident || item_ident(*this, ISFLAG_KNOW_PROPERTIES))
           && is_artefact(*this) && special != UNRAND_OCTOPUS_KING_RING
           && base_type != OBJ_GIZMOS)
    {
        // Artefacts always get "the" unless we just want the plain name.
        switch (descrip)
        {
        default:
            buff << "the ";
        case DESC_PLAIN:
        case DESC_DBNAME:
        case DESC_BASENAME:
        case DESC_QUALNAME:
            break;
        }
    }
    else if (quantity > 1 || always_plural)
    {
        switch (descrip)
        {
        case DESC_THE:        buff << "the "; break;
        case DESC_YOUR:       buff << "your "; break;
        case DESC_ITS:        buff << "its "; break;
        case DESC_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
        case DESC_PLAIN:
        default:
            break;
        }

        if (descrip != DESC_BASENAME && descrip != DESC_QUALNAME
            && descrip != DESC_DBNAME && !always_plural)
        {
            if (quantity_in_words)
                buff << number_in_words(quantity) << " ";
            else
                buff << quantity << " ";
        }
    }
    else
    {
        switch (descrip)
        {
        case DESC_THE:        buff << "the "; break;
        case DESC_YOUR:       buff << "your "; break;
        case DESC_ITS:        buff << "its "; break;
        case DESC_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
                              buff << (startvowel ? "an " : "a "); break;
        case DESC_PLAIN:
        default:
            break;
        }
    }

    buff << auxname;

    if (descrip == DESC_INVENTORY_EQUIP)
    {
        equipment_type eq = item_equip_slot(*this);
        if (eq != EQ_NONE)
        {
            if (you.melded[eq])
                buff << " (melded)";
            else
            {
                switch (eq)
                {
                case EQ_WEAPON:
                    if (is_weapon(*this))
                        buff << " (weapon)";
                    else if (you.has_mutation(MUT_NO_GRASPING))
                        buff << " (in mouth)";
                    else
                        buff << " (in " << you.hand_name(false) << ")";
                    break;
                case EQ_OFFHAND:
                    if (is_weapon(*this))
                    {
                        buff << " (offhand)";
                        break;
                    }
                    // fallthrough to EQ_CLOAK...EQ_BODY_ARMOUR
                case EQ_CLOAK:
                case EQ_HELMET:
                case EQ_GLOVES:
                case EQ_BOOTS:
                case EQ_BODY_ARMOUR:
                    buff << " (worn)";
                    break;
                case EQ_LEFT_RING:
                case EQ_RIGHT_RING:
                case EQ_RING_ONE:
                case EQ_RING_TWO:
                    buff << " (";
                    buff << ((eq == EQ_LEFT_RING || eq == EQ_RING_ONE)
                             ? "left" : "right");
                    buff << " ";
                    buff << you.hand_name(false);
                    buff << ")";
                    break;
                case EQ_AMULET:
                    if (you.species == SP_OCTOPODE && form_keeps_mutations())
                        buff << " (around mantle)";
                    else
                        buff << " (around neck)";
                    break;
                case EQ_RING_THREE:
                case EQ_RING_FOUR:
                case EQ_RING_FIVE:
                case EQ_RING_SIX:
                case EQ_RING_SEVEN:
                case EQ_RING_EIGHT:
                    buff << " (on tentacle)";
                    break;
                case EQ_RING_AMULET:
                    buff << " (on amulet)";
                    break;
                case EQ_GIZMO:
                    buff << " (installed)";
                    break;
                default:
                    die("Item in an invalid slot");
                }
            }
        }
        else if (base_type == OBJ_TALISMANS
                 && you.using_talisman(*this))
        {
                buff << " (active)";
        }
        else if (you.quiver_action.item_is_quivered(*this))
            buff << " (quivered)";
    }

    if (descrip != DESC_BASENAME && descrip != DESC_DBNAME && with_inscription)
        buff << _item_inscription(*this);

    // These didn't have "cursed " prepended; add them here so that
    // it comes after the inscription.
    if (terse && descrip != DESC_DBNAME && descrip != DESC_BASENAME
        && !qualname
        && is_artefact(*this) && cursed())
    {
        buff << " (curse)";
    }

    return buff.str();
}

static bool _missile_brand_is_prefix(special_missile_type brand)
{
    switch (brand)
    {
    case SPMSL_POISONED:
    case SPMSL_CURARE:
    case SPMSL_BLINDING:
    case SPMSL_FRENZY:
    case SPMSL_EXPLODING:
#if TAG_MAJOR_VERSION == 34
    case SPMSL_STEEL:
#endif
    case SPMSL_SILVER:
        return true;
    default:
        return false;
    }
}

static bool _missile_brand_is_postfix(special_missile_type brand)
{
    return brand != SPMSL_NORMAL && !_missile_brand_is_prefix(brand);
}

const char* missile_brand_name(const item_def &item, mbn_type t)
{
    const special_missile_type brand
        = static_cast<special_missile_type>(item.brand);
    switch (brand)
    {
#if TAG_MAJOR_VERSION == 34
    case SPMSL_FLAME:
        return "flame";
    case SPMSL_FROST:
        return "frost";
#endif
    case SPMSL_POISONED:
        return t == MBN_NAME ? "poisoned" : "poison";
    case SPMSL_CURARE:
        return t == MBN_NAME ? "curare-tipped" : "curare";
#if TAG_MAJOR_VERSION == 34
    case SPMSL_EXPLODING:
        return t == MBN_TERSE ? "explode" : "exploding";
    case SPMSL_STEEL:
        return "steel";
    case SPMSL_RETURNING:
        return t == MBN_TERSE ? "return" : "returning";
    case SPMSL_PENETRATION:
        return t == MBN_TERSE ? "penet" : "penetration";
#endif
    case SPMSL_SILVER:
        return "silver";
#if TAG_MAJOR_VERSION == 34
    case SPMSL_PARALYSIS:
        return "paralysis";
    case SPMSL_SLOW:
        return t == MBN_TERSE ? "slow" : "slowing";
    case SPMSL_SLEEP:
        return t == MBN_TERSE ? "sleep" : "sleeping";
    case SPMSL_CONFUSION:
        return t == MBN_TERSE ? "conf" : "confusion";
    case SPMSL_SICKNESS:
        return t == MBN_TERSE ? "sick" : "sickness";
#endif
    case SPMSL_FRENZY:
        return t == MBN_NAME ? "datura-tipped" : "datura";
    case SPMSL_CHAOS:
        return "chaos";
    case SPMSL_DISPERSAL:
        return t == MBN_TERSE ? "disperse" : "dispersal";
    case SPMSL_BLINDING:
        return t == MBN_NAME ? "atropa-tipped" : "atropa";
    case SPMSL_NORMAL:
        return "";
    default:
        return t == MBN_TERSE ? "buggy" : "bugginess";
    }
}

static const char *weapon_brands_terse[] =
{
    "", "flame", "freeze", "holy", "elec",
#if TAG_MAJOR_VERSION == 34
    "obsolete", "obsolete",
#endif
    "venom", "protect", "drain", "speed", "heavy",
#if TAG_MAJOR_VERSION == 34
    "obsolete", "obsolete",
#endif
    "vamp", "pain", "antimagic", "distort",
#if TAG_MAJOR_VERSION == 34
    "obsolete", "obsolete",
#endif
    "chaos",
#if TAG_MAJOR_VERSION == 34
    "evade", "confuse",
#endif
    "penet", "reap", "spect", "num_special", "acid",
#if TAG_MAJOR_VERSION > 34
    "confuse",
#endif
    "weak",
    "vuln",
    "foul flame",
    "debug",
};

static const char *weapon_brands_verbose[] =
{
    "", "flaming", "freezing", "holy wrath", "electrocution",
#if TAG_MAJOR_VERSION == 34
    "orc slaying", "dragon slaying",
#endif
    "venom", "protection", "draining", "speed", "heavy",
#if TAG_MAJOR_VERSION == 34
    "flame", "frost",
#endif
    "vampirism", "pain", "antimagic", "distortion",
#if TAG_MAJOR_VERSION == 34
    "reaching", "returning",
#endif
    "chaos",
#if TAG_MAJOR_VERSION == 34
    "evasion", "confusion",
#endif
    "penetration", "reaping", "spectralizing", "num_special", "acid",
#if TAG_MAJOR_VERSION > 34
    "confusion",
#endif
    "weakness",
    "vulnerability",
    "foul flame",
    "debug",
};

static const char *weapon_brands_adj[] =
{
    "", "flaming", "freezing", "holy", "electric",
#if TAG_MAJOR_VERSION == 34
    "orc-killing", "dragon-slaying",
#endif
    "venomous", "protective", "draining", "fast", "heavy",
#if TAG_MAJOR_VERSION == 34
    "flaming", "freezing",
#endif
    "vampiric", "painful", "antimagic", "distorting",
#if TAG_MAJOR_VERSION == 34
    "reaching", "returning",
#endif
    "chaotic",
#if TAG_MAJOR_VERSION == 34
    "evasive", "confusing",
#endif
    "penetrating", "reaping", "spectral", "num_special", "acidic",
#if TAG_MAJOR_VERSION > 34
    "confusing",
#endif
    "weakening",
    "will-reducing",
    "foul flame",
    "debug",
};

COMPILE_CHECK(ARRAYSZ(weapon_brands_terse) == NUM_SPECIAL_WEAPONS);
COMPILE_CHECK(ARRAYSZ(weapon_brands_verbose) == NUM_SPECIAL_WEAPONS);
COMPILE_CHECK(ARRAYSZ(weapon_brands_adj) == NUM_SPECIAL_WEAPONS);

static const set<brand_type> brand_prefers_adj =
            { SPWPN_VAMPIRISM, SPWPN_ANTIMAGIC, SPWPN_HEAVY, SPWPN_SPECTRAL };

/**
 * What's the name of a type of weapon brand?
 *
 * @param brand             The type of brand in question.
 * @param bool              Whether to use a terse or verbose name.
 * @return                  The name of the given brand.
 */
const char* brand_type_name(brand_type brand, bool terse)
{
    if (brand < 0 || brand >= NUM_SPECIAL_WEAPONS)
        return terse ? "buggy" : "bugginess";

    return (terse ? weapon_brands_terse : weapon_brands_verbose)[brand];
}

const char* brand_type_adj(brand_type brand)
{
    if (brand < 0 || brand >= NUM_SPECIAL_WEAPONS)
        return "buggy";

    return weapon_brands_adj[brand];
}

/**
 * What's the name of a given weapon's brand?
 *
 * @param item              The weapon with the brand.
 * @param bool              Whether to use a terse or verbose name.
 * @param override_brand    A brand to use instead of the weapon's actual brand.
 * @return                  The name of the given item's brand.
 */
const char* weapon_brand_name(const item_def& item, bool terse,
                              brand_type override_brand)
{
    const brand_type brand = override_brand ? override_brand : get_weapon_brand(item);

    return brand_type_name(brand, terse);
}

const char* special_armour_type_name(special_armour_type ego, bool terse)
{
    if (!terse)
    {
        switch (ego)
        {
        case SPARM_NORMAL:            return "";
#if TAG_MAJOR_VERSION == 34
        case SPARM_RUNNING:           return "running";
#endif
        case SPARM_FIRE_RESISTANCE:   return "fire resistance";
        case SPARM_COLD_RESISTANCE:   return "cold resistance";
        case SPARM_POISON_RESISTANCE: return "poison resistance";
        case SPARM_SEE_INVISIBLE:     return "see invisible";
        case SPARM_INVISIBILITY:      return "invisibility";
        case SPARM_STRENGTH:          return "strength";
        case SPARM_DEXTERITY:         return "dexterity";
        case SPARM_INTELLIGENCE:      return "intelligence";
        case SPARM_PONDEROUSNESS:     return "ponderousness";
        case SPARM_FLYING:            return "flying";

        case SPARM_WILLPOWER:         return "willpower";
        case SPARM_PROTECTION:        return "protection";
        case SPARM_STEALTH:           return "stealth";
        case SPARM_RESISTANCE:        return "resistance";
        case SPARM_POSITIVE_ENERGY:   return "positive energy";
        case SPARM_ARCHMAGI:          return "the Archmagi";
#if TAG_MAJOR_VERSION == 34
        case SPARM_JUMPING:           return "jumping";
#endif
        case SPARM_PRESERVATION:      return "preservation";
        case SPARM_REFLECTION:        return "reflection";
        case SPARM_SPIRIT_SHIELD:     return "spirit shield";
        case SPARM_HURLING:           return "hurling";
        case SPARM_REPULSION:         return "repulsion";
#if TAG_MAJOR_VERSION == 34
        case SPARM_CLOUD_IMMUNE:      return "cloud immunity";
#endif
        case SPARM_HARM:              return "harm";
        case SPARM_SHADOWS:           return "shadows";
        case SPARM_RAMPAGING:         return "rampaging";
        case SPARM_INFUSION:          return "infusion";
        case SPARM_LIGHT:             return "light";
        case SPARM_RAGE:              return "wrath";
        case SPARM_MAYHEM:            return "mayhem";
        case SPARM_GUILE:             return "guile";
        case SPARM_ENERGY:            return "energy";
        default:                      return "bugginess";
        }
    }
    else
    {
        switch (ego)
        {
        case SPARM_NORMAL:            return "";
#if TAG_MAJOR_VERSION == 34
        case SPARM_RUNNING:           return "obsolete";
#endif
        case SPARM_FIRE_RESISTANCE:   return "rF+";
        case SPARM_COLD_RESISTANCE:   return "rC+";
        case SPARM_POISON_RESISTANCE: return "rPois";
        case SPARM_SEE_INVISIBLE:     return "SInv";
        case SPARM_INVISIBILITY:      return "+Inv";
        case SPARM_STRENGTH:          return "Str+3";
        case SPARM_DEXTERITY:         return "Dex+3";
        case SPARM_INTELLIGENCE:      return "Int+3";
        case SPARM_PONDEROUSNESS:     return "ponderous";
        case SPARM_FLYING:            return "Fly";
        case SPARM_WILLPOWER:         return "Will+";
        case SPARM_PROTECTION:        return "AC+3";
        case SPARM_STEALTH:           return "Stlth+";
        case SPARM_RESISTANCE:        return "rC+ rF+";
        case SPARM_POSITIVE_ENERGY:   return "rN+";
        case SPARM_ARCHMAGI:          return "Archmagi";
#if TAG_MAJOR_VERSION == 34
        case SPARM_JUMPING:           return "obsolete";
#endif
        case SPARM_PRESERVATION:      return "rCorr";
        case SPARM_REFLECTION:        return "reflect";
        case SPARM_SPIRIT_SHIELD:     return "Spirit";
        case SPARM_HURLING:           return "hurl";
        case SPARM_REPULSION:         return "repulsion";
#if TAG_MAJOR_VERSION == 34
        case SPARM_CLOUD_IMMUNE:      return "obsolete";
#endif
        case SPARM_HARM:              return "harm";
        case SPARM_SHADOWS:           return "shadows";
        case SPARM_RAMPAGING:         return "rampage";
        case SPARM_INFUSION:          return "infuse";
        case SPARM_LIGHT:             return "light";
        case SPARM_RAGE:              return "*Rage";
        case SPARM_MAYHEM:            return "mayhem";
        case SPARM_GUILE:             return "guile";
        case SPARM_ENERGY:            return "*channel";
        default:                      return "buggy";
        }
    }
}

const char* armour_ego_name(const item_def& item, bool terse)
{
    return special_armour_type_name(get_armour_ego_type(item), terse);
}

static const char* _wand_type_name(int wandtype)
{
    switch (wandtype)
    {
    case WAND_FLAME:           return "flame";
    case WAND_PARALYSIS:       return "paralysis";
    case WAND_DIGGING:         return "digging";
    case WAND_ICEBLAST:        return "iceblast";
    case WAND_POLYMORPH:       return "polymorph";
    case WAND_CHARMING:        return "charming";
    case WAND_ACID:            return "acid";
    case WAND_MINDBURST:       return "mindburst";
    case WAND_LIGHT:           return "light";
    case WAND_QUICKSILVER:     return "quicksilver";
    case WAND_ROOTS:           return "roots";
    case WAND_WARPING:         return "warping";
    default:                   return item_type_removed(OBJ_WANDS, wandtype)
                                    ? "removedness"
                                    : "bugginess";
    }
}

static const char* wand_secondary_string(uint32_t s)
{
    static const char* const secondary_strings[] = {
        "", "jewelled ", "curved ", "long ", "short ", "twisted ", "crooked ",
        "forked ", "shiny ", "blackened ", "tapered ", "glowing ", "worn ",
        "encrusted ", "runed ", "sharpened "
    };
    COMPILE_CHECK(ARRAYSZ(secondary_strings) == NDSC_WAND_SEC);
    return secondary_strings[s % NDSC_WAND_SEC];
}

static const char* wand_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        "iron", "brass", "bone", "wooden", "copper", "gold", "silver",
        "bronze", "ivory", "glass", "lead", "fluorescent"
    };
    COMPILE_CHECK(ARRAYSZ(primary_strings) == NDSC_WAND_PRI);
    return primary_strings[p % NDSC_WAND_PRI];
}

const char* potion_type_name(int potiontype)
{
    switch (static_cast<potion_type>(potiontype))
    {
    case POT_CURING:            return "curing";
    case POT_HEAL_WOUNDS:       return "heal wounds";
    case POT_HASTE:             return "haste";
    case POT_MIGHT:             return "might";
    case POT_ATTRACTION:        return "attraction";
    case POT_BRILLIANCE:        return "brilliance";
    case POT_ENLIGHTENMENT:     return "enlightenment";
    case POT_CANCELLATION:      return "cancellation";
    case POT_AMBROSIA:          return "ambrosia";
    case POT_INVISIBILITY:      return "invisibility";
    case POT_DEGENERATION:      return "degeneration";
    case POT_EXPERIENCE:        return "experience";
    case POT_MAGIC:             return "magic";
    case POT_BERSERK_RAGE:      return "berserk rage";
    case POT_MUTATION:          return "mutation";
    case POT_RESISTANCE:        return "resistance";
    case POT_LIGNIFY:           return "lignification";

    // FIXME: Remove this once known-items no longer uses this as a sentinel.
    default:
                                return "bugginess";
    CASE_REMOVED_POTIONS(potiontype); // TODO: this will crash, is that correct??
    }
}

static const char* scroll_type_name(int scrolltype)
{
    switch (static_cast<scroll_type>(scrolltype))
    {
    case SCR_IDENTIFY:           return "identify";
    case SCR_TELEPORTATION:      return "teleportation";
    case SCR_FEAR:               return "fear";
    case SCR_NOISE:              return "noise";
    case SCR_SUMMONING:          return "summoning";
    case SCR_ENCHANT_WEAPON:     return "enchant weapon";
    case SCR_ENCHANT_ARMOUR:     return "enchant armour";
    case SCR_TORMENT:            return "torment";
    case SCR_IMMOLATION:         return "immolation";
    case SCR_POISON:             return "poison";
    case SCR_BUTTERFLIES:        return "butterflies";
    case SCR_BLINKING:           return "blinking";
    case SCR_REVELATION:         return "revelation";
    case SCR_FOG:                return "fog";
    case SCR_ACQUIREMENT:        return "acquirement";
    case SCR_BRAND_WEAPON:       return "brand weapon";
    case SCR_VULNERABILITY:      return "vulnerability";
    case SCR_SILENCE:            return "silence";
    case SCR_AMNESIA:            return "amnesia";
#if TAG_MAJOR_VERSION == 34
    case SCR_HOLY_WORD:          return "holy word";
    case SCR_CURSE_WEAPON:       return "curse weapon";
    case SCR_CURSE_ARMOUR:       return "curse armour";
    case SCR_CURSE_JEWELLERY:    return "curse jewellery";
#endif
    default:                     return item_type_removed(OBJ_SCROLLS,
                                                          scrolltype)
                                     ? "removedness"
                                     : "bugginess";
    }
}

/**
 * Get the name for the effect provided by a kind of jewellery.
 *
 * @param jeweltype     The jewellery_type of the item in question.
 * @return              A string describing the effect of the given jewellery
 *                      subtype.
 */
const char* jewellery_effect_name(int jeweltype, bool terse)
{
    if (!terse)
    {
        switch (static_cast<jewellery_type>(jeweltype))
        {
#if TAG_MAJOR_VERSION == 34
        case RING_REGENERATION:          return "obsoleteness";
        case RING_ATTENTION:             return "obsoleteness";
#endif
        case RING_PROTECTION:            return "protection";
        case RING_PROTECTION_FROM_FIRE:  return "protection from fire";
        case RING_POISON_RESISTANCE:     return "poison resistance";
        case RING_PROTECTION_FROM_COLD:  return "protection from cold";
        case RING_STRENGTH:              return "strength";
        case RING_SLAYING:               return "slaying";
        case RING_SEE_INVISIBLE:         return "see invisible";
        case RING_RESIST_CORROSION:      return "resist corrosion";
        case RING_EVASION:               return "evasion";
#if TAG_MAJOR_VERSION == 34
        case RING_SUSTAIN_ATTRIBUTES:    return "sustain attributes";
#endif
        case RING_STEALTH:               return "stealth";
        case RING_DEXTERITY:             return "dexterity";
        case RING_INTELLIGENCE:          return "intelligence";
        case RING_WIZARDRY:              return "wizardry";
        case RING_MAGICAL_POWER:         return "magical power";
        case RING_FLIGHT:                return "flight";
        case RING_POSITIVE_ENERGY:       return "positive energy";
        case RING_WILLPOWER: return "willpower";
        case RING_FIRE:                  return "fire";
        case RING_ICE:                   return "ice";
#if TAG_MAJOR_VERSION == 34
        case RING_TELEPORTATION:         return "teleportation";
        case RING_TELEPORT_CONTROL:      return "teleport control";
#endif
        case AMU_MANA_REGENERATION: return "magic regeneration";
        case AMU_ACROBAT:           return "the acrobat";
#if TAG_MAJOR_VERSION == 34
        case AMU_RAGE:              return "rage";
        case AMU_THE_GOURMAND:      return "gourmand";
        case AMU_HARM:              return "harm";
        case AMU_CONSERVATION:      return "conservation";
        case AMU_CONTROLLED_FLIGHT: return "controlled flight";
        case AMU_INACCURACY:        return "inaccuracy";
#endif
        case AMU_NOTHING:           return "nothing";
        case AMU_GUARDIAN_SPIRIT:   return "guardian spirit";
        case AMU_FAITH:             return "faith";
        case AMU_REFLECTION:        return "reflection";
        case AMU_REGENERATION:      return "regeneration";
        default: return "buggy jewellery";
        }
    }
    else
    {
        if (jewellery_base_ability_string(jeweltype)[0] != '\0')
            return jewellery_base_ability_string(jeweltype);
        switch (static_cast<jewellery_type>(jeweltype))
        {
#if TAG_MAJOR_VERSION == 34
        case RING_REGENERATION:          return "obsoleteness";
        case RING_ATTENTION:             return "obsoleteness";
#endif
        case RING_PROTECTION:            return "AC";
        case RING_PROTECTION_FROM_FIRE:  return "rF+";
        case RING_POISON_RESISTANCE:     return "rPois";
        case RING_PROTECTION_FROM_COLD:  return "rC+";
        case RING_STRENGTH:              return "Str";
        case RING_SLAYING:               return "Slay";
        case RING_SEE_INVISIBLE:         return "sInv";
        case RING_RESIST_CORROSION:      return "rCorr";
        case RING_EVASION:               return "EV";
        case RING_STEALTH:               return "Stlth+";
        case RING_DEXTERITY:             return "Dex";
        case RING_INTELLIGENCE:          return "Int";
        case RING_MAGICAL_POWER:         return "MP+9";
        case RING_FLIGHT:                return "Fly";
        case RING_POSITIVE_ENERGY:       return "rN+";
        case RING_WILLPOWER:             return "Will+";
        case RING_WIZARDRY:              return "Wiz";
        case AMU_REGENERATION:           return "Regen";
        case AMU_MANA_REGENERATION:      return "RegenMP";
#if TAG_MAJOR_VERSION == 34
        case AMU_RAGE:                   return "+Rage";
#endif
        case AMU_ACROBAT:                return "Acrobat";
        case AMU_NOTHING:                return "";
        default: return "buggy";
        }
    }
}

/**
 * Get the name for the category of a type of jewellery.
 *
 * @param jeweltype     The jewellery_type of the item in question.
 * @return              A string describing the kind of jewellery it is.
 */
static const char* _jewellery_class_name(int jeweltype)
{
#if TAG_MAJOR_VERSION == 34
    if (jeweltype == RING_REGENERATION)
        return "ring of";
#endif

    if (jeweltype < RING_FIRST_RING || jeweltype >= NUM_JEWELLERY
        || jeweltype >= NUM_RINGS && jeweltype < AMU_FIRST_AMULET)
    {
        return "buggy"; // "buggy buggy jewellery"
    }

    if (jeweltype < NUM_RINGS)
        return "ring of";
    return "amulet of";
}

/**
 * Get the name for a type of jewellery.
 *
 * @param jeweltype     The jewellery_type of the item in question.
 * @return              The full name of the jewellery type in question.
 */
static string jewellery_type_name(int jeweltype)
{
    return make_stringf("%s %s", _jewellery_class_name(jeweltype),
                                 jewellery_effect_name(jeweltype));
}


static const char* ring_secondary_string(uint32_t s)
{
    static const char* const secondary_strings[] = {
        "", "encrusted ", "glowing ", "tubular ", "runed ", "blackened ",
        "scratched ", "small ", "large ", "twisted ", "shiny ", "notched ",
        "knobbly "
    };
    COMPILE_CHECK(ARRAYSZ(secondary_strings) == NDSC_JEWEL_SEC);
    return secondary_strings[s % NDSC_JEWEL_SEC];
}

static const char* ring_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        "wooden", "silver", "golden", "iron", "steel", "tourmaline", "brass",
        "copper", "granite", "ivory", "ruby", "marble", "jade", "glass",
        "agate", "bone", "diamond", "emerald", "peridot", "garnet", "opal",
        "pearl", "coral", "sapphire", "cabochon", "gilded", "onyx", "bronze",
        "moonstone"
    };
    COMPILE_CHECK(ARRAYSZ(primary_strings) == NDSC_JEWEL_PRI);
    return primary_strings[p % NDSC_JEWEL_PRI];
}

static const char* amulet_secondary_string(uint32_t s)
{
    static const char* const secondary_strings[] = {
        "dented ", "square ", "thick ", "thin ", "runed ", "blackened ",
        "glowing ", "small ", "large ", "twisted ", "tiny ", "triangular ",
        "lumpy "
    };
    COMPILE_CHECK(ARRAYSZ(secondary_strings) == NDSC_JEWEL_SEC);
    return secondary_strings[s % NDSC_JEWEL_SEC];
}

static const char* amulet_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        "sapphire", "zirconium", "golden", "emerald", "garnet", "bronze",
        "brass", "copper", "ruby", "citrine", "bone", "platinum", "jade",
        "fluorescent", "amethyst", "cameo", "pearl", "blue", "peridot",
        "jasper", "diamond", "malachite", "steel", "cabochon", "silver",
        "soapstone", "lapis lazuli", "filigree", "beryl"
    };
    COMPILE_CHECK(ARRAYSZ(primary_strings) == NDSC_JEWEL_PRI);
    return primary_strings[p % NDSC_JEWEL_PRI];
}

const char* rune_type_name(short p)
{
    switch (static_cast<rune_type>(p))
    {
    case RUNE_DIS:         return "iron";
    case RUNE_GEHENNA:     return "obsidian";
    case RUNE_COCYTUS:     return "icy";
    case RUNE_TARTARUS:    return "bone";
    case RUNE_SLIME:       return "slimy";
    case RUNE_VAULTS:      return "silver";
    case RUNE_SNAKE:       return "serpentine";
    case RUNE_ELF:         return "elven";
    case RUNE_TOMB:        return "golden";
    case RUNE_SWAMP:       return "decaying";
    case RUNE_SHOALS:      return "barnacled";
    case RUNE_SPIDER:      return "gossamer";
    case RUNE_FOREST:      return "mossy";

    // pandemonium and abyss runes:
    case RUNE_DEMONIC:     return "demonic";
    case RUNE_ABYSSAL:     return "abyssal";

    // special pandemonium runes:
    case RUNE_MNOLEG:      return "glowing";
    case RUNE_LOM_LOBON:   return "magical";
    case RUNE_CEREBOV:     return "fiery";
    case RUNE_GLOORX_VLOQ: return "dark";
    default:               return "buggy";
    }
}

static string gem_type_name(gem_type g)
{
    return string(gem_adj(g)) + " gem";
}

static string misc_type_name(int type)
{
#if TAG_MAJOR_VERSION == 34
    if (is_deck_type(type))
        return "removed deck";
#endif

    switch (static_cast<misc_item_type>(type))
    {
#if TAG_MAJOR_VERSION == 34
    case MISC_CRYSTAL_BALL_OF_ENERGY:    return "removed crystal ball";
#endif
    case MISC_BOX_OF_BEASTS:             return "box of beasts";
#if TAG_MAJOR_VERSION == 34
    case MISC_BUGGY_EBONY_CASKET:        return "removed ebony casket";
    case MISC_FAN_OF_GALES:              return "removed fan of gales";
    case MISC_LAMP_OF_FIRE:              return "removed lamp of fire";
    case MISC_BUGGY_LANTERN_OF_SHADOWS:  return "removed lantern of shadows";
#endif
    case MISC_HORN_OF_GERYON:            return "horn of Geryon";
    case MISC_LIGHTNING_ROD:             return "lightning rod";
#if TAG_MAJOR_VERSION == 34
    case MISC_BOTTLED_EFREET:            return "empty flask";
    case MISC_RUNE_OF_ZOT:               return "obsolete rune of zot";
    case MISC_STONE_OF_TREMORS:          return "removed stone of tremors";
#endif
    case MISC_QUAD_DAMAGE:               return "quad damage";
    case MISC_PHIAL_OF_FLOODS:           return "phial of floods";
    case MISC_SACK_OF_SPIDERS:           return "sack of spiders";
    case MISC_PHANTOM_MIRROR:            return "phantom mirror";
    case MISC_ZIGGURAT:                  return "figurine of a ziggurat";
#if TAG_MAJOR_VERSION == 34
    case MISC_XOMS_CHESSBOARD:           return "removed chess piece";
#endif
    case MISC_TIN_OF_TREMORSTONES:       return "tin of tremorstones";
    case MISC_CONDENSER_VANE:            return "condenser vane";
    case MISC_GRAVITAMBOURINE:           return "Gell's gravitambourine";

    default:
        return "buggy miscellaneous item";
    }
}

const char* gizmo_effect_name(int type)
{
    switch (static_cast<special_gizmo_type>(type))
    {
        case SPGIZMO_MANAREV:       return "RevMPSaver";
        case SPGIZMO_GADGETEER:     return "Gadgeteer";
        case SPGIZMO_PARRYREV:      return "RevParry";
        case SPGIZMO_AUTODAZZLE:    return "AutoDazzle";

        default:
        case SPGIZMO_NORMAL:        return "";
    }
}

static const char* _book_type_name(int booktype)
{
    switch (static_cast<book_type>(booktype))
    {
    case BOOK_MINOR_MAGIC:            return "Minor Magic";
    case BOOK_CONJURATIONS:           return "Conjurations";
    case BOOK_FLAMES:                 return "Flames";
    case BOOK_FROST:                  return "Frost";
    case BOOK_WILDERNESS:             return "the Wilderness";
    case BOOK_FIRE:                   return "Fire";
    case BOOK_ICE:                    return "Ice";
    case BOOK_SPATIAL_TRANSLOCATIONS: return "Spatial Translocations";
    case BOOK_HEXES:                  return "Hexes";
    case BOOK_LIGHTNING:              return "Lightning";
    case BOOK_DEATH:                  return "Death";
    case BOOK_MISFORTUNE:             return "Misfortune";
    case BOOK_SPONTANEOUS_COMBUSTION: return "Spontaneous Combustion";
#if TAG_MAJOR_VERSION == 34
    case BOOK_TRANSFIGURATIONS:       return "Transfigurations";
#endif
    case BOOK_BATTLE:                 return "Battle";
    case BOOK_VAPOURS:                return "Vapours";
    case BOOK_NECROMANCY:             return "Necromancy";
    case BOOK_CALLINGS:               return "Callings";
#if TAG_MAJOR_VERSION == 34
    case BOOK_MALEDICT:               return "Maledictions";
#endif
    case BOOK_AIR:                    return "Air";
#if TAG_MAJOR_VERSION == 34
    case BOOK_SKY:                    return "the Sky";
#endif
    case BOOK_WARP:                   return "the Warp";
#if TAG_MAJOR_VERSION == 34
    case BOOK_ENVENOMATIONS:          return "Envenomations";
#endif
    case BOOK_ANNIHILATIONS:          return "Annihilations";
    case BOOK_UNLIFE:                 return "Unlife";
#if TAG_MAJOR_VERSION == 34
    case BOOK_CONTROL:                return "Control";
#endif
    case BOOK_GEOMANCY:               return "Geomancy";
    case BOOK_EARTH:                  return "the Earth";
#if TAG_MAJOR_VERSION == 34
    case BOOK_WIZARDRY:               return "Wizardry";
#endif
    case BOOK_POWER:                  return "Power";
    case BOOK_CANTRIPS:               return "Cantrips";
    case BOOK_PARTY_TRICKS:           return "Party Tricks";
    case BOOK_DEBILITATION:           return "Debilitation";
    case BOOK_DRAGON:                 return "the Dragon";
    case BOOK_BURGLARY:               return "Burglary";
    case BOOK_DREAMS:                 return "Dreams";
    case BOOK_TRANSMUTATION:         return "Transmutation";
    case BOOK_BEASTS:                 return "Beasts";
    case BOOK_SPECTACLE:              return "Spectacle";
    case BOOK_WINTER:                 return "Winter";
    case BOOK_SPHERES:                return "the Spheres";
    case BOOK_ARMAMENTS:              return "Armaments";
#if TAG_MAJOR_VERSION == 34
    case BOOK_PAIN:                   return "Pain";
#endif
    case BOOK_DECAY:                  return "Decay";
    case BOOK_DISPLACEMENT:           return "Displacement";
#if TAG_MAJOR_VERSION == 34
    case BOOK_RIME:                   return "Rime";
    case BOOK_STONE:                  return "Stone";
#endif
    case BOOK_SENSES:                 return "the Senses";
    case BOOK_BLASTING:               return "Blasting";
    case BOOK_IRON:                   return "Iron";
    case BOOK_TUNDRA:                 return "the Tundra";
    case BOOK_MOON:                   return "the Moon";
    case BOOK_STORMS:                 return "Storms";
    case BOOK_WEAPONS:                return "Weapons";
    case BOOK_SLOTH:                  return "Sloth";
    case BOOK_BLOOD:                  return "Blood";
    case BOOK_DANGEROUS_FRIENDS:      return "Dangerous Friends";
    case BOOK_TOUCH:                  return "Touch";
    case BOOK_CHAOS:                  return "Chaos";
    case BOOK_HUNTER:                 return "the Hunter";
    case BOOK_SCORCHING:              return "Scorching";
    case BOOK_MOVEMENT:               return "Movement";
    case BOOK_WICKED_CREATION:        return "Wicked Creation";
    case BOOK_MALADIES:               return "Maladies";
    case BOOK_RANDART_LEVEL:          return "Fixed Level";
    case BOOK_RANDART_THEME:          return "Fixed Theme";
    default:                          return "Bugginess";
    }
}

static const char* staff_secondary_string(uint32_t s)
{
    static const char* const secondary_strings[] = {
        "crooked ", "knobbly ", "weird ", "gnarled ", "thin ", "curved ",
        "twisted ", "thick ", "long ", "short ",
    };
    COMPILE_CHECK(NDSC_STAVE_SEC == ARRAYSZ(secondary_strings));
    return secondary_strings[s % ARRAYSZ(secondary_strings)];
}

static const char* staff_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        "glowing ", "jewelled ", "runed ", "smoking "
    };
    COMPILE_CHECK(NDSC_STAVE_PRI == ARRAYSZ(primary_strings));
    return primary_strings[p % ARRAYSZ(primary_strings)];
}

const char *base_type_string(const item_def &item)
{
    return base_type_string(item.base_type);
}

const char *base_type_string(object_class_type type)
{
    switch (type)
    {
    case OBJ_WEAPONS: return "weapon";
    case OBJ_MISSILES: return "missile";
    case OBJ_ARMOUR: return "armour";
    case OBJ_WANDS: return "wand";
    case OBJ_SCROLLS: return "scroll";
    case OBJ_JEWELLERY: return "jewellery";
    case OBJ_POTIONS: return "potion";
    case OBJ_BOOKS: return "book";
    case OBJ_STAVES: return "staff";
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS: return "removed rod";
#endif
    case OBJ_ORBS: return "orb";
    case OBJ_MISCELLANY: return "miscellaneous";
    case OBJ_CORPSES: return "corpse";
    case OBJ_GOLD: return "gold";
    case OBJ_RUNES: return "rune";
    case OBJ_GEMS: return "gem";
    case OBJ_TALISMANS: return "talisman";
    case OBJ_GIZMOS: return "gizmo";
    default: return "";
    }
}

string sub_type_string(const item_def &item, bool known)
{
    const object_class_type type = item.base_type;
    const int sub_type = item.sub_type;

    switch (type)
    {
    case OBJ_WEAPONS:  // deliberate fall through, as XXX_prop is a local
    case OBJ_MISSILES: // variable to item-prop.cc.
    case OBJ_ARMOUR:
        return item_base_name(type, sub_type);
    case OBJ_WANDS: return _wand_type_name(sub_type);
    case OBJ_SCROLLS: return scroll_type_name(sub_type);
    case OBJ_JEWELLERY: return jewellery_type_name(sub_type);
    case OBJ_POTIONS: return potion_type_name(sub_type);
    case OBJ_STAVES: return staff_type_name(static_cast<stave_type>(sub_type));
    case OBJ_BOOKS:
    {
        switch (sub_type)
        {
        case BOOK_MANUAL:
            {
            if (!known)
                return "manual";
            string bookname = "manual of ";
            bookname += skill_name(static_cast<skill_type>(item.plus));
            return bookname;
            }
        case BOOK_NECRONOMICON:
            return "Necronomicon";
        case BOOK_GRAND_GRIMOIRE:
            return "Grand Grimoire";
#if TAG_MAJOR_VERSION == 34
        case BOOK_BUGGY_DESTRUCTION:
            return "tome of obsoleteness";
#endif
        case BOOK_EVERBURNING:
            // Aus. English apparently follows the US spelling, not UK.
            return "Everburning Encyclopedia";
#if TAG_MAJOR_VERSION == 34
        case BOOK_OZOCUBU:
            return "Ozocubu's Autobiography";
#endif
        case BOOK_MAXWELL:
            return "Maxwell's Memoranda";
        case BOOK_YOUNG_POISONERS:
            return "Young Poisoner's Handbook";
        case BOOK_FEN:
            return "Fen Folio";
#if TAG_MAJOR_VERSION == 34
        case BOOK_NEARBY:
            return "Inescapable Atlas";
#endif
        case BOOK_THERE_AND_BACK:
            return "There-And-Back Book";
        case BOOK_BIOGRAPHIES_II:
            return "Great Wizards, Vol. II";
        case BOOK_BIOGRAPHIES_VII:
            return "Great Wizards, Vol. VII";
        case BOOK_TRISMEGISTUS:
            return "Trismegistus Codex";
        case BOOK_UNRESTRAINED:
            return "the Unrestrained Analects";
        case BOOK_SIEGECRAFT:
            return "Compendium of Siegecraft";
        case BOOK_CONDUCTIVITY:
            return "Codex of Conductivity";
#if TAG_MAJOR_VERSION == 34
        case BOOK_AKASHIC_RECORD:
            return "Akashic Record";
#endif
        default:
            return string("book of ") + _book_type_name(sub_type);
        }
    }
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:   return "removed rod";
#endif
    case OBJ_MISCELLANY: return misc_type_name(sub_type);
    case OBJ_TALISMANS: return talisman_type_name(sub_type);
    // these repeat as base_type_string
    case OBJ_ORBS: return "orb of Zot";
    case OBJ_CORPSES: return "corpse";
    case OBJ_GOLD: return "gold";
    case OBJ_RUNES: return "rune of Zot";
    case OBJ_GEMS: return gem_type_name(static_cast<gem_type>(sub_type));
    default: return "";
    }
}

/**
 * What's the name for the weapon used by a given ghost / pan lord?
 *
 * There's no actual weapon info, just brand, so we have to improvise...
 *
 * @param brand     The brand_type used by the ghost or pan lord.
 * @param mtype     Monster type; determines whether the fake weapon is
 *                  described as a `weapon` or a `touch`.
 * @return          The name of the ghost's weapon (e.g. "weapon of flaming",
 *                  "antimagic weapon"). SPWPN_NORMAL returns "".
 */
string ghost_brand_name(brand_type brand, monster_type mtype)
{
    if (brand == SPWPN_NORMAL)
        return "";
    const bool weapon = mtype != MONS_PANDEMONIUM_LORD;
    if (weapon)
    {
        // n.b. heavy only works if it is adjectival
        if (brand_prefers_adj.count(brand))
            return make_stringf("%s weapon", brand_type_adj(brand));
        else
            return make_stringf("weapon of %s", brand_type_name(brand, false));
    }
    else
        return make_stringf("%s touch", brand_type_adj(brand));
}

string ego_type_string(const item_def &item, bool terse)
{
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return armour_ego_name(item, terse);
    case OBJ_WEAPONS:
        if (get_weapon_brand(item) != SPWPN_NORMAL)
            return weapon_brand_name(item, terse);
        else
            return "";
    case OBJ_MISSILES:
        // HACKHACKHACK
        if (item.props.exists(DAMNATION_BOLT_KEY))
            return "damnation";
        return missile_brand_name(item, terse ? MBN_TERSE : MBN_BRAND);
    case OBJ_JEWELLERY:
        return jewellery_effect_name(item.sub_type, terse);
    case OBJ_GIZMOS:
        return gizmo_effect_name(item.brand);
    default:
        return "";
    }
}

/**
 * When naming the given item, should the base name be used?
 */
static bool _use_basename(const item_def &item, description_level_type desc,
                          bool ident)
{
    const bool know_type = ident || item_type_known(item);
    return desc == DESC_BASENAME
           || desc == DESC_DBNAME && !know_type;
}

/**
 * When naming the given item, should identifiable properties be mentioned?
 */
static bool _know_any_ident(const item_def &item, description_level_type desc,
                            bool ident)
{
    return desc != DESC_QUALNAME && desc != DESC_DBNAME
           && !_use_basename(item, desc, ident);
}

/**
 * When naming the given item, should the specified identifiable property be
 * mentioned?
 */
static bool _know_ident(const item_def &item, description_level_type desc,
                        bool ident, iflags_t ignore_flags,
                        item_status_flag_type vprop)
{
    return _know_any_ident(item, desc, ident)
            && !testbits(ignore_flags, vprop)
            && (ident || item_ident(item, vprop));
}

/**
 * When naming the given item, should the pluses be mentioned?
 */
static bool _know_pluses(const item_def &item, description_level_type desc,
                          bool ident, iflags_t ignore_flags)
{
    return _know_ident(item, desc, ident, ignore_flags, ISFLAG_KNOW_PLUSES);
}

/**
 * When naming the given item, should the brand be mentioned?
 */
static bool _know_ego(const item_def &item, description_level_type desc,
                         bool ident, iflags_t ignore_flags)
{
    return _know_any_ident(item, desc, ident)
           && !testbits(ignore_flags, ISFLAG_KNOW_TYPE)
           && (ident || item_type_known(item));
}

/**
 * The plus-describing prefix to a weapon's name, including trailing space.
 */
static string _plus_prefix(const item_def &weap)
{
    if (is_unrandom_artefact(weap, UNRAND_WOE))
        return Options.char_set == CSET_ASCII ? "+inf " : "+\u221e "; // ∞
    return make_stringf("%+d ", weap.plus);
}

/**
 * Cosmetic text for weapons (e.g. glowing, runed). Includes trailing space,
 * if appropriate. (Empty if there is no cosmetic property, or if it's
 * marked to be ignored.)
 */
static string _cosmetic_text(const item_def &weap, iflags_t ignore_flags)
{
    const iflags_t desc = get_equip_desc(weap);
    if (testbits(ignore_flags, desc))
        return "";

    switch (desc)
    {
        case ISFLAG_RUNED:
            return "runed ";
        case ISFLAG_GLOWING:
            return "glowing ";
        default:
            return "";
    }
}

/**
 * Surrounds a given string with the weapon's brand-describing prefix/suffix
 * as appropriate.
 */
string weapon_brand_desc(const char *body, const item_def &weap,
                         bool terse, brand_type override_brand)
{

    const string brand_name = weapon_brand_name(weap, terse, override_brand);

    if (brand_name.empty())
        return body;

    if (terse)
        return make_stringf("%s (%s)", body, brand_name.c_str());

    const brand_type brand = override_brand ? override_brand :
                             get_weapon_brand(weap);

    if (brand_prefers_adj.count(brand))
        return make_stringf("%s %s", brand_type_adj(brand), body);
    else if (brand == SPWPN_NORMAL)
    {
        if (get_equip_desc(weap))
            return make_stringf("enchanted %s", body);
        else
            return body;
    }
    else
        return make_stringf("%s of %s", body, brand_name.c_str());
}

/**
 * Build the appropriate name for a given weapon.
 *
 * @param weap          The weapon in question.
 * @param desc          The type of name to provide. (E.g. the name to be used
 *                      in database lookups for description, or...)
 * @param terse         Whether to provide a terse version of the name for
 *                      display in the HUD.
 * @param ident         Whether the weapon should be named as if it were
 *                      identified.
 * @param inscr         Whether an inscription will be added later.
 * @param ignore_flags  Identification flags on the weapon to ignore.
 *
 * @return              A name for the weapon.
 *                      TODO: example
 */
static string _name_weapon(const item_def &weap, description_level_type desc,
                           bool terse, bool ident, bool inscr,
                           iflags_t ignore_flags)
{
    const bool dbname   = (desc == DESC_DBNAME);
    const bool basename = _use_basename(weap, desc, ident);
    const bool qualname = (desc == DESC_QUALNAME);

    const bool know_pluses = _know_pluses(weap, desc, ident, ignore_flags);
    const bool know_ego =    _know_ego(weap, desc, ident, ignore_flags);

    const string curse_prefix = !dbname && !terse && weap.cursed() ? "cursed " : "";
    const string plus_text = know_pluses ? _plus_prefix(weap) : "";
    const string chaotic = testbits(weap.flags, ISFLAG_CHAOTIC) ? "chaotic " : "";

    if (is_artefact(weap) && !dbname)
    {
        const string long_name = curse_prefix + chaotic + plus_text
                                 + get_artefact_name(weap, ident);

        // crop long artefact names when not controlled by webtiles -
        // webtiles displays weapon names across multiple lines
#ifdef USE_TILE_WEB
        if (!tiles.is_controlled_from_web())
#endif
        {
            const bool has_inscript = desc != DESC_BASENAME
                                   && desc != DESC_DBNAME
                                   && inscr;
            const string inscription = _item_inscription(weap);

            const int total_length = long_name.size()
                                     + (has_inscript ? inscription.size() : 0);
            const string inv_slot_text = "x) ";
            const int max_length = crawl_view.hudsz.x - inv_slot_text.size();
            if (!terse || total_length <= max_length)
                return long_name;
        }
#ifdef USE_TILE_WEB
        else
            return long_name;
#endif

        // special case: these two shouldn't ever have their base name revealed
        // (since showing 'eudaemon blade' is unhelpful in the former case, and
        // showing 'broad axe' is misleading in the latter)
        // could be a flag, but doesn't seem worthwhile for only two items
        if (is_unrandom_artefact(weap, UNRAND_ZEALOT_SWORD)
            || is_unrandom_artefact(weap, UNRAND_DEMON_AXE))
        {
            return long_name;
        }

        const string short_name
            = curse_prefix + plus_text + get_artefact_base_name(weap, true);
        return short_name;
    }

    const bool show_cosmetic = !basename && !qualname && !dbname
                               && !know_pluses && !know_ego
                               && !terse
                               && !(ignore_flags & ISFLAG_COSMETIC_MASK);

    const string cosmetic_text
        = show_cosmetic ? _cosmetic_text(weap, ignore_flags) : "";
    const string base_name = item_base_name(weap);
    const string name_with_ego
        = know_ego ? weapon_brand_desc(base_name.c_str(), weap, terse)
        : base_name;
    const string curse_suffix
        = weap.cursed() && terse && !dbname && !qualname ? " (curse)" :  "";
    return curse_prefix + plus_text + cosmetic_text
           + name_with_ego + curse_suffix;
}

// Note that "terse" is only currently used for the "in hand" listing on
// the game screen.
string item_def::name_aux(description_level_type desc, bool terse, bool ident,
                          bool with_inscription, iflags_t ignore_flags) const
{
    // Shortcuts
    const int item_typ   = sub_type;

    const bool know_type = ident || item_type_known(*this);

    const bool dbname   = (desc == DESC_DBNAME);
    const bool basename = _use_basename(*this, desc, ident);
    const bool qualname = (desc == DESC_QUALNAME);

    const bool know_pluses = _know_pluses(*this, desc, ident, ignore_flags);
    const bool know_brand =  _know_ego(*this, desc, ident, ignore_flags);

    const bool know_ego = know_brand;

    // Display runed/glowing/embroidered etc?
    // Only display this if brand is unknown.
    const bool show_cosmetic = !know_pluses && !know_brand
                               && !basename && !qualname && !dbname
                               && !terse
                               && !(ignore_flags & ISFLAG_COSMETIC_MASK);

    const bool need_plural = !basename && !dbname;

    ostringstream buff;

    switch (base_type)
    {
    case OBJ_WEAPONS:
        buff << _name_weapon(*this, desc, terse, ident, with_inscription,
                             ignore_flags);
        break;

    case OBJ_MISSILES:
    {
        special_missile_type msl_brand = get_ammo_brand(*this);

        if (!terse && !dbname && !basename)
        {
            if (props.exists(DAMNATION_BOLT_KEY)) // hack alert
                buff << "damnation ";
            else if (_missile_brand_is_prefix(msl_brand)) // see below for postfix brands
                buff << missile_brand_name(*this, MBN_NAME) << ' ';
        }

        buff << ammo_name(static_cast<missile_type>(item_typ));

        if (msl_brand != SPMSL_NORMAL
            && !basename && !dbname)
        {
            if (terse)
            {
                if (props.exists(DAMNATION_BOLT_KEY)) // still a hack
                    buff << " (damnation)";
                else
                    buff << " (" <<  missile_brand_name(*this, MBN_TERSE) << ")";
            }
            else if (_missile_brand_is_postfix(msl_brand)) // see above for prefix brands
                buff << " of " << missile_brand_name(*this, MBN_NAME);
        }

        break;
    }
    case OBJ_ARMOUR:
        if (!terse && cursed())
            buff << "cursed ";

        // If we know enough to know it has *something* ('shiny' etc),
        // but we know it has no ego, it must have a plus. (or maybe a curse.)
        // If we don't know what the plus is, call it 'enchanted'.
        if (!terse && know_ego && get_armour_ego_type(*this) == SPARM_NORMAL &&
            !know_pluses && !is_artefact(*this) && get_equip_desc(*this))
        {
            buff << "enchanted ";
        }

        // Don't list unenchantable armor as +0.
        if (know_pluses && armour_is_enchantable(*this))
            buff << make_stringf("%+d ", plus);

        if ((item_typ == ARM_GLOVES || item_typ == ARM_BOOTS)
            && !is_unrandom_artefact(*this, UNRAND_POWER_GLOVES)
            && !is_unrandom_artefact(*this, UNRAND_DELATRAS_GLOVES))
        {
            buff << "pair of ";
        }

        if (is_artefact(*this) && !dbname)
        {
            buff << get_artefact_name(*this, ident);
            break;
        }

        if (show_cosmetic)
        {
            switch (get_equip_desc(*this))
            {
            case ISFLAG_EMBROIDERED_SHINY:
                if (testbits(ignore_flags, ISFLAG_EMBROIDERED_SHINY))
                    break;
                if (item_typ == ARM_ROBE || item_typ == ARM_CLOAK
                    || item_typ == ARM_GLOVES || item_typ == ARM_BOOTS
                    || item_typ == ARM_SCARF
                    || get_armour_slot(*this) == EQ_HELMET
                       && !is_hard_helmet(*this))
                {
                    buff << "embroidered ";
                }
                else if (item_typ != ARM_LEATHER_ARMOUR
                         && item_typ != ARM_ANIMAL_SKIN)
                {
                    buff << "shiny ";
                }
                else
                    buff << "dyed ";
                break;

            case ISFLAG_RUNED:
                if (!testbits(ignore_flags, ISFLAG_RUNED))
                    buff << "runed ";
                break;

            case ISFLAG_GLOWING:
                if (!testbits(ignore_flags, ISFLAG_GLOWING))
                    buff << "glowing ";
                break;
            }
        }

        buff << item_base_name(*this);

        if (know_ego && !is_artefact(*this))
        {
            const special_armour_type sparm = get_armour_ego_type(*this);

            if (sparm != SPARM_NORMAL)
            {
                if (!terse)
                    buff << " of ";
                else
                    buff << " {";
                buff << armour_ego_name(*this, terse);
                if (terse)
                    buff << "}";
            }
        }

        if (cursed() && terse && !dbname && !qualname)
            buff << " (curse)";
        break;

    case OBJ_WANDS:
        if (basename)
        {
            buff << "wand";
            break;
        }

        if (know_type)
            buff << "wand of " << _wand_type_name(item_typ);
        else
        {
            buff << wand_secondary_string(subtype_rnd / NDSC_WAND_PRI)
                 << wand_primary_string(subtype_rnd % NDSC_WAND_PRI)
                 << " wand";
        }

        if (dbname)
            break;

        if (know_type && charges > 0)
            buff << " (" << charges << ")";

        break;

    case OBJ_POTIONS:
        if (basename)
        {
            buff << "potion";
            break;
        }

        if (know_type)
            buff << "potion of " << potion_type_name(item_typ);
        else
        {
            const int pqual   = PQUAL(subtype_rnd);
            const int pcolour = PCOLOUR(subtype_rnd);

            static const char *potion_qualifiers[] =
            {
                "",  "bubbling ", "fuming ", "fizzy ", "viscous ", "lumpy ",
                "smoky ", "glowing ", "sedimented ", "metallic ", "murky ",
                "gluggy ", "oily ", "slimy ", "emulsified ",
            };
            COMPILE_CHECK(ARRAYSZ(potion_qualifiers) == PDQ_NQUALS);

            static const char *potion_colours[] =
            {
#if TAG_MAJOR_VERSION == 34
                "clear",
#endif
                "blue", "black", "silvery", "cyan", "purple", "orange",
                "inky", "red", "yellow", "green", "brown", "ruby", "white",
                "emerald", "grey", "pink", "coppery", "golden", "dark", "puce",
                "amethyst", "sapphire",
            };
            COMPILE_CHECK(ARRAYSZ(potion_colours) == PDC_NCOLOURS);

            const char *qualifier =
                (pqual < 0 || pqual >= PDQ_NQUALS) ? "bug-filled "
                                    : potion_qualifiers[pqual];

            const char *clr =  (pcolour < 0 || pcolour >= PDC_NCOLOURS) ?
                                   "bogus" : potion_colours[pcolour];

            buff << qualifier << clr << " potion";
        }
        break;

#if TAG_MAJOR_VERSION == 34
    case OBJ_FOOD:
        buff << "removed food"; break;
        break;
#endif

    case OBJ_SCROLLS:
        buff << "scroll";
        if (basename)
            break;
        else
            buff << " ";

        if (know_type)
            buff << "of " << scroll_type_name(item_typ);
        else
            buff << "labelled " << make_name(subtype_rnd, MNAME_SCROLL);
        break;

    case OBJ_JEWELLERY:
    {
        if (basename)
        {
            if (jewellery_is_amulet(*this))
                buff << "amulet";
            else
                buff << "ring";

            break;
        }

        const bool is_randart = is_artefact(*this);

        if (!terse && cursed())
            buff << "cursed ";

        if (is_randart && !dbname)
        {
            buff << get_artefact_name(*this, ident);
            if (!ident
                && !item_ident(*this, ISFLAG_KNOW_PROPERTIES)
                && item_type_known(*this))
            {
                buff << " of " << jewellery_effect_name(item_typ);
            }
            break;
        }

        if (know_type)
        {
            if (!dbname && jewellery_has_pluses(*this))
                buff << make_stringf("%+d ", plus);

            buff << jewellery_type_name(item_typ);
        }
        else
        {
            if (jewellery_is_amulet(*this))
            {
                buff << amulet_secondary_string(subtype_rnd / NDSC_JEWEL_PRI)
                     << amulet_primary_string(subtype_rnd % NDSC_JEWEL_PRI)
                     << " amulet";
            }
            else  // i.e., a ring
            {
                buff << ring_secondary_string(subtype_rnd / NDSC_JEWEL_PRI)
                     << ring_primary_string(subtype_rnd % NDSC_JEWEL_PRI)
                     << " ring";
            }
        }
        if (cursed() && terse && !dbname && !qualname)
            buff << " (curse)";
        break;
    }
    case OBJ_MISCELLANY:
    {
        if (!dbname && item_typ == MISC_ZIGGURAT && you.zigs_completed > 0)
            buff << "+" << you.zigs_completed << " ";

        buff << misc_type_name(item_typ);

        if (is_xp_evoker(*this) && !dbname && !evoker_charges(sub_type))
            buff << " (inert)";
        else if (is_xp_evoker(*this) &&
                 !dbname && evoker_max_charges(sub_type) > 1)
        {
            buff << " (" << evoker_charges(sub_type) << "/"
                 << evoker_max_charges(sub_type) << ")";
        }

        break;
    }

    case OBJ_TALISMANS:
        if (is_random_artefact(*this) && !dbname && !basename)
            buff << get_artefact_name(*this, ident);
        else
            buff << talisman_type_name(item_typ);
        break;

    case OBJ_BOOKS:
        if (is_random_artefact(*this) && !dbname && !basename)
        {
            buff << get_artefact_name(*this, true);
            break;
        }
        if (basename)
            buff << (item_typ == BOOK_MANUAL ? "manual" : "book");
        else
            buff << sub_type_string(*this, !dbname);
        break;

#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
        buff << "removed rod";
        break;
#endif

    case OBJ_STAVES:
        if (!terse && cursed())
            buff << "cursed ";

        if (is_artefact(*this) && !dbname)
        {
            if (know_type)
                buff << "staff";
            // TODO: crop long artefact names when not controlled by webtiles
            buff << get_artefact_name(*this, ident);
            if (!know_type)
                buff << "staff";
        }
        else if (!know_type)
        {
            if (!basename)
            {
                buff << staff_secondary_string(subtype_rnd / NDSC_STAVE_PRI)
                     << staff_primary_string(subtype_rnd % NDSC_STAVE_PRI);
            }

            buff << "staff";
        }
        else
            buff << "staff of " << staff_type_name(static_cast<stave_type>(item_typ));

        if (cursed() && terse && !dbname && !qualname)
            buff << " (curse)";
        break;

    // rearranged 15 Apr 2000 {dlb}:
    case OBJ_ORBS:
        buff.str("Orb of Zot");
        break;

    case OBJ_RUNES:
        if (!dbname)
            buff << rune_type_name(sub_type) << " ";
        buff << "rune of Zot";
        break;

    case OBJ_GEMS:
        if (sub_type == NUM_GEM_TYPES)
            buff << "gem";
        else
            buff << gem_type_name(static_cast<gem_type>(sub_type));
        break;

    case OBJ_GOLD:
        buff << "gold piece";
        break;

    case OBJ_CORPSES:
    {
        if (dbname && item_typ == CORPSE_SKELETON)
            return "decaying skeleton";

        monster_flags_t name_flags;
        const string _name = get_corpse_name(*this, &name_flags);
        const monster_flags_t name_type = name_flags & MF_NAME_MASK;

        const bool shaped = starts_with(_name, "shaped ");

        if (!_name.empty() && name_type == MF_NAME_ADJECTIVE)
            buff << _name << " ";

        if ((name_flags & MF_NAME_SPECIES) && name_type == MF_NAME_REPLACE)
            buff << _name << " ";
        else if (!dbname && !starts_with(_name, "the "))
        {
            const monster_type mc = mon_type;
            if (!(mons_is_unique(mc) && mons_species(mc) == mc))
                buff << mons_type_name(mc, DESC_PLAIN) << ' ';

            if (!_name.empty() && shaped)
                buff << _name << ' ';
        }

        if (item_typ == CORPSE_BODY)
            buff << "corpse";
        else if (item_typ == CORPSE_SKELETON)
            buff << "skeleton";
        else
            buff << "corpse bug";

        if (!_name.empty() && !shaped && name_type != MF_NAME_ADJECTIVE
            && !(name_flags & MF_NAME_SPECIES) && name_type != MF_NAME_SUFFIX
            && !dbname)
        {
            buff << " of " << _name;
        }
        break;
    }

    case OBJ_GIZMOS:
    {
        if (props.exists(ARTEFACT_NAME_KEY))
            buff << props[ARTEFACT_NAME_KEY].get_string();
        else
            buff << "Unnamed gizmo";
    }
    break;

    default:
        buff << "!";
    }

    // One plural to rule them all.
    if (need_plural && quantity > 1 && !basename && !qualname)
        buff.str(pluralise(buff.str()));

    // debugging output -- oops, I probably block it above ... dang! {dlb}
    if (buff.str().length() < 3)
    {
        buff << "bad item (cl:" << static_cast<int>(base_type)
             << ",ty:" << item_typ << ",pl:" << plus
             << ",pl2:" << plus2 << ",sp:" << special
             << ",qu:" << quantity << ")";
    }

    return buff.str();
}

// WARNING: You can break save compatibility if you edit this without
// amending tags.cc to properly marshall the change.
bool item_type_has_ids(object_class_type base_type)
{
    COMPILE_CHECK(NUM_WEAPONS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_MISSILES   < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_ARMOURS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_WANDS      < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_SCROLLS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_JEWELLERY  < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_POTIONS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_STAVES     < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_MISCELLANY < MAX_SUBTYPES);
#if TAG_MAJOR_VERSION == 34
    COMPILE_CHECK(NUM_RODS       < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_FOODS      < MAX_SUBTYPES);
#endif
    // no check for NUM_BOOKS (which exceeds MAX_SUBTYPES), not used here

    return base_type == OBJ_WANDS || base_type == OBJ_SCROLLS
        || base_type == OBJ_JEWELLERY || base_type == OBJ_POTIONS
        || base_type == OBJ_STAVES;
}

bool item_brand_known(const item_def& item)
{
    return item_ident(item, ISFLAG_KNOW_TYPE)
           || is_artefact(item)
           && artefact_known_property(item, ARTP_BRAND);
}

bool item_type_known(const item_def& item)
{
    if (item_ident(item, ISFLAG_KNOW_TYPE))
        return true;

    switch (item.base_type)
    {
    case OBJ_MISCELLANY:
    case OBJ_MISSILES:
    case OBJ_BOOKS:
        return true;
    default:
        break;
    }

    // Artefacts have different descriptions from other items,
    // so we can't use general item knowledge for them.
    if (is_artefact(item))
        return false;

    if (!item_type_has_ids(item.base_type))
        return false;
    return you.type_ids[item.base_type][item.sub_type];
}

bool item_type_unknown(const item_def& item)
{
    if (item_type_known(item))
        return false;

    if (is_artefact(item))
        return true;

    return item_type_has_ids(item.base_type);
}

bool item_type_known(const object_class_type base_type, const int sub_type)
{
    if (!item_type_has_ids(base_type))
        return false;
    return you.type_ids[base_type][sub_type];
}

// Has every item a scroll can identify that is worth identifying
// (i.e. all scrolls and potions for non-mummies) been identified by "you"?
void check_if_everything_is_identified()
{
    vector<object_class_type> types = { OBJ_SCROLLS };

    if (you.can_drink(false))
        types.push_back(OBJ_POTIONS);

    for (const auto t : types)
    {
        ASSERT(item_type_has_ids(t));
        int unidentified = 0;

        for (const auto s : all_item_subtypes(t))
        {
            if (!item_type_known(t, s)
                && !item_known_excluded_from_set(t, s)
                && unidentified++)
            {
                you.props.erase(IDENTIFIED_ALL_KEY);
                return;
            }
        }
    }
    you.props[IDENTIFIED_ALL_KEY] = true;
}

bool set_ident_type(item_def &item, bool identify, bool check_last)
{
    if (is_artefact(item) || crawl_state.game_is_arena())
        return false;

    if (!set_ident_type(item.base_type, item.sub_type, identify, check_last))
        return false;

    if (in_inventory(item))
    {
        shopping_list.cull_identical_items(item);
        if (identify)
            item_skills(item, you.skills_to_show);
    }

    if (identify && notes_are_active()
        && is_interesting_item(item)
        && !(item.flags & (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET)))
    {
        // Make a note of it.
        take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_A),
                       origin_desc(item)));

        // Sometimes (e.g. shops) you can ID an item before you get it;
        // don't note twice in those cases.
        item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
    }

    return true;
}

bool set_ident_type(object_class_type basetype, int subtype, bool identify,
                    bool check_last)
{
    if (!item_type_has_ids(basetype))
        return false;

    if (you.type_ids[basetype][subtype] == identify)
        return false;

    you.type_ids[basetype][subtype] = identify;
    maybe_mark_set_known(basetype, subtype);
    request_autoinscribe();

    // Our item knowledge changed in a way that could possibly affect shop
    // prices.
    shopping_list.item_type_identified(basetype, subtype);

    // We identified something, maybe we identified other things by process of
    // elimination.
    if (identify && !(you.pending_revival || crawl_state.updating_scores))
        _maybe_identify_pack_item();

    if (check_last)
        check_if_everything_is_identified();

    return true;
}

bool get_ident_type(const item_def &item)
{
    if (is_artefact(item))
        return false;

    return get_ident_type(item.base_type, item.sub_type);
}

bool get_ident_type(object_class_type basetype, int subtype)
{
    if (!item_type_has_ids(basetype))
        return false;
    ASSERT(subtype < MAX_SUBTYPES);
    return you.type_ids[basetype][subtype];
}

static colour_t _gem_colour(const item_def *gem)
{
    if (!you.gems_found[gem->sub_type])
        return DARKGREY;
    return gem->gem_colour();
}

static string _gem_parenthetical(gem_type gem)
{
    string text = " (";
    text += branches[branch_for_gem(gem)].longname;

    const int lim = gem_time_limit(gem);
    const int left = lim - you.gem_time_spent[gem];

    // We need to check time left rather than shattered, since the latter is
    // only set when the gem is actually broken, and we may not have loaded
    // the relevant level since we ran out of time.
    if (left <= 0)
    {
        if (Options.more_gem_info || !you.gems_found[gem])
            return text + ", shattered)";
        return text + ")";
    }

    if (!gem_clock_active()
        || !Options.more_gem_info && you.gems_found[gem])
    {
        return text + ")";
    }

    // Rescale from aut to dAut. Round up.
    text += make_stringf(", %d", (left + 9) / 10);
    if (left < lim)
        text += make_stringf("/%d", (lim + 9) / 10);
    else
        text += " turns"; // XXX: ?
    return text + " until shattered)";
}

static string _gem_text(const item_def *gem_it)
{
    string text = gem_it->name(DESC_PLAIN);
    const gem_type gem = static_cast<gem_type>(gem_it->sub_type);
    text = colourize_str(text, _gem_colour(gem_it));
    const bool in_branch = player_in_branch(branch_for_gem(gem));
    const colour_t pcol = in_branch ? WHITE
              : you.gems_found[gem] ? LIGHTGREY
                                    : DARKGREY;
    text += colourize_str(_gem_parenthetical(gem), pcol);
    return text;
}

static MenuEntry* _fixup_runeorb_entry(MenuEntry* me)
{
    auto entry = static_cast<InvEntry*>(me);
    ASSERT(entry);

    switch (entry->item->base_type)
    {
    case OBJ_RUNES:
    {
        auto rune = static_cast<rune_type>(entry->item->sub_type);
        colour_t colour;
        // Make Gloorx's rune more distinguishable from uncollected runes.
        if (you.runes[rune])
        {
            colour = (rune == RUNE_GLOORX_VLOQ) ? colour_t{LIGHTGREY}
                                                : rune_colour(rune);
        }
        else
            colour = DARKGREY;

        string text = "<";
        text += colour_to_str(colour);
        text += ">";
        text += rune_type_name(rune);
        text += " rune of Zot";
        if (!you.runes[rune])
        {
            text += " (";
            text += branches[rune_location(rune)].longname;
            text += ")";
        }
        text += "</";
        text += colour_to_str(colour);
        text += ">";
        entry->text = text;
        break;
    }
    case OBJ_GEMS:
        entry->text = _gem_text(entry->item);
        break;
    case OBJ_ORBS:
        if (player_has_orb())
            entry->text = "<magenta>The Orb of Zot</magenta>";
        else
        {
            entry->text = "<darkgrey>The Orb of Zot"
            " (the Realm of Zot)</darkgrey>";
        }
        break;
    default:
        entry->text = "Eggplant"; // bug!
        break;
    }

    return entry;
}

class RuneMenu : public InvMenu
{
    virtual bool process_key(int keyin) override;

public:
    RuneMenu();

private:
    void populate();

    string get_title();
    string gem_title();

    void fill_contents();
    void set_normal_runes();
    void set_sprint_runes();
    void set_gems();

    void set_footer();

    bool can_show_gems();
    bool can_show_more_gems();

    bool show_gems;
    // For player morale, default to hiding gems you've already missed.
    bool more_gems;

    vector<item_def> contents;
};

RuneMenu::RuneMenu()
    : InvMenu(MF_NOSELECT | MF_ALLOW_FORMATTING),
      show_gems(false), more_gems(false)
{
    populate();
}

void RuneMenu::populate()
{
    contents.clear();
    items.clear();

    set_title(get_title());
    fill_contents();
    // We've sorted this vector already, so disable menu sorting. Maybe we
    // could a menu entry comparator and modify InvMenu::load_items() to allow
    // passing this in instead of doing a sort ahead of time.
    load_items(contents, _fixup_runeorb_entry, 0, false);

    set_footer();
}

string RuneMenu::get_title()
{
    if (show_gems)
        return gem_title();

    auto col = runes_in_pack() < ZOT_ENTRY_RUNES ?  "lightgrey" :
               runes_in_pack() < you.obtainable_runes ? "green" :
                                                   "lightgreen";

    return make_stringf("<white>Runes of Zot (</white>"
                        "<%s>%d</%s><white> collected) & Orbs of Power</white>",
                        col, runes_in_pack(), col);
}

string RuneMenu::gem_title()
{
    const int found = gems_found();
    const int lost = gems_lost();
    string gem_title = make_stringf("<white>Gems (%d collected", found);
    if (Options.more_gem_info && lost < found)
        gem_title += make_stringf(", %d intact", found - lost);
    // don't explicitly mention that your gems are all broken otherwise - sad!

    return gem_title + ")</white>";
}

void RuneMenu::set_footer()
{
    if (!can_show_gems())
        return;

    string more_text = make_stringf("[<w>!</w>/<w>^</w>"
#ifdef USE_TILE_LOCAL
            "|<w>Right-click</w>"
#endif
            "]: %s", show_gems ? "Show Runes" : "Show Gems");
    if (!Options.more_gem_info && can_show_more_gems())
        more_text += make_stringf("\n[<w>-</w>]: %s", more_gems ? "Less" : "More");
    set_more(more_text);
}

bool RuneMenu::can_show_gems()
{
    return !crawl_state.game_is_sprint() || !crawl_state.game_is_descent();
}

bool RuneMenu::can_show_more_gems()
{
    if (!show_gems)
        return false;
    for (int i = 0; i < NUM_GEM_TYPES; i++)
        if (you.gems_shattered[i] && !you.gems_found[i])
            return true;
    return false;
}

void RuneMenu::fill_contents()
{
    if (show_gems)
    {
        set_gems();
        return;
    }

    item_def item;
    item.base_type = OBJ_ORBS;
    item.sub_type = ORB_ZOT;
    item.quantity = player_has_orb() ? 1 : 0;
    contents.push_back(item);

    if (crawl_state.game_is_sprint())
        set_sprint_runes();
    else
        set_normal_runes();
}

void RuneMenu::set_normal_runes()
{
    // Add the runes in order of challenge (semi-arbitrary).
    for (branch_iterator it(branch_iterator_type::danger); it; ++it)
    {
        const branch_type br = it->id;
        if (!connected_branch_can_exist(br))
            continue;

        for (auto rune : branches[br].runes)
        {
            item_def item;
            item.base_type = OBJ_RUNES;
            item.sub_type = rune;
            item.quantity = you.runes[rune] ? 1 : 0;
            ::item_colour(item);
            contents.push_back(item);
        }
    }
}

void RuneMenu::set_sprint_runes()
{
    // We don't know what runes are accessible in the sprint, so just show
    // the ones you have. We can't iterate over branches as above since the
    // elven rune and mossy rune may exist in sprint.
    for (int i = 0; i < NUM_RUNE_TYPES; ++i)
    {
        if (!you.runes[i])
            continue;

        item_def item;
        item.base_type = OBJ_RUNES;
        item.sub_type = i;
        item.quantity = 1;
        ::item_colour(item);
        contents.push_back(item);
    }
}

void RuneMenu::set_gems()
{
    // Add the gems in order of challenge (semi-arbitrary).
    for (branch_iterator it(branch_iterator_type::danger); it; ++it)
    {
        const branch_type br = it->id;
        if (!connected_branch_can_exist(br))
            continue;
        const gem_type gem = gem_for_branch(br);
        if (gem == NUM_GEM_TYPES)
            continue;

        if (!Options.more_gem_info
            && !more_gems
            && !you.gems_found[gem]
            // We need to check time left rather than shattered, since the latter is
            // only set when the gem is actually broken, and we may not have loaded
            // the relevant level since we ran out of time.
            && you.gem_time_spent[gem] >= gem_time_limit(gem))
        {
            continue;
        }

        item_def item;
        item.base_type = OBJ_GEMS;
        item.sub_type = gem;
        item.quantity = you.gems_found[gem] ? 1 : 0;
        ::item_colour(item);
        contents.push_back(item);
    }
}

bool RuneMenu::process_key(int keyin)
{
    if (!can_show_gems())
        return Menu::process_key(keyin);

    switch (keyin)
    {
    case '!':
    case '^':
    case CK_MOUSE_CMD:
        show_gems = !show_gems;
        populate();
        update_menu(true);
        return true;
    case '-':
        if (show_gems)
        {
            more_gems = !more_gems;
            populate();
            update_menu(true);
            return true;
        }
        return Menu::process_key(keyin);
    default:
        return Menu::process_key(keyin);
    }
}

void display_runes()
{
    RuneMenu().show();
}

static string _unforbid(string name)
{
    // Ironically, the ROT13'd versions can be pretty good names.
    set<string> forbidden_words = set<string>{
        "puvax", "snt", "avt", "avttre",
        "xvxr", "ovgpu", "juber", "tvzc",
        "ergneq", "phag", "pbba", "fdhnj",
        "jbt", "qlxr", "ubzb", "genaal"
    };
    auto parts = split_string(" ", name);
    for (size_t i = 0; i < parts.size(); i++)
    {
        string part = parts[i];
        string rot13d = "";
        for (size_t j = 0; j < part.size(); j++)
            rot13d += ((part[j] + 13 - 'a') % 26) + 'a';
        if (forbidden_words.count(rot13d))
            parts[i] = rot13d;
    }
    return join_strings(parts.begin(), parts.end());
}

// Seed ranges for _random_consonant_set: (B)eginning and one-past-the-(E)nd
// of the (B)eginning, (E)nding, and (M)iddle cluster ranges.
const size_t RCS_BB = 0;
const size_t RCS_EB = 27;
const size_t RCS_BE = 14;
const size_t RCS_EE = 56;
const size_t RCS_BM = 0;
const size_t RCS_EM = 67;
const size_t RCS_END = RCS_EM;

#define ITEMNAME_SIZE 200
/**
 * Make a random name from the given seed.
 *
 * Used for: Pandemonium demonlords, shopkeepers, scrolls, random artefacts.
 *
 * This function is insane, but that might be useful.
 *
 * @param seed      The seed to generate the name from.
 *                  The same seed will always generate the same name.
 *                  By default a random number from the current RNG.
 * @param name_type The type of name to be generated.
 *                  If MNAME_SCROLL, increase length by 6 and force to allcaps.
 *                  If MNAME_JIYVA, start with J, do not generate spaces,
 *                  recurse instead of ploggifying, and cap length at 8.
 *                  Otherwise, no special behaviour.
 * @return          A randomly generated name.
 *                  E.g. "Joiduir", "Jays Fya", ZEFOKY WECZYXE,
 *                  THE GIAGGOSTUONO, etc.
 */
string make_name(uint32_t seed, makename_type name_type)
{
    // use the seed to select sequence, rather than seed per se. This is
    // because it is not important that the sequence be randomly distributed
    // in uint64_t.
    rng::subgenerator subgen(you.game_seed, static_cast<uint64_t>(seed));

    string name;

    bool has_space  = false; // Keep track of whether the name contains a space.

    size_t len = 3;
    len += random2(5);
    len += (random2(5) == 0) ? random2(6) : 1;

    if (name_type == MNAME_SCROLL)   // scrolls have longer names
        len += 6;

    const size_t maxlen = name_type == MNAME_JIYVA ? 8 : SIZE_MAX;
    len = min(len, maxlen);

    ASSERT_RANGE(len, 1, ITEMNAME_SIZE + 1);

    static const int MAX_ITERS = 150;
    for (int iters = 0; iters < MAX_ITERS && name.length() < len; ++iters)
    {
        const char prev_char = name.length() ? name[name.length() - 1]
                                              : '\0';
        const char penult_char = name.length() > 1 ? name[name.length() - 2]
                                                    : '\0';
        if (name.empty() && name_type == MNAME_JIYVA)
        {
            // Start the name with a predefined letter.
            name += 'j';
        }
        else if (name.empty() || prev_char == ' ')
        {
            // Start the word with any letter.
            name += 'a' + random2(26);
        }
        else if (!has_space && name_type != MNAME_JIYVA
                 && name.length() > 5 && name.length() < len - 4
                 && random2(5) != 0) // 4/5 chance
        {
             // Hand out a space.
            name += ' ';
        }
        else if (name.length()
                 && (_is_consonant(prev_char)
                     || (name.length() > 1
                         && !_is_consonant(prev_char)
                         && _is_consonant(penult_char)
                         && random2(5) <= 1))) // 2/5
        {
            // Place a vowel.
            const char vowel = _random_vowel();

            if (vowel == ' ')
            {
                if (len < 7
                         || name.length() <= 2 || name.length() >= len - 3
                         || prev_char == ' ' || penult_char == ' '
                         || name_type == MNAME_JIYVA
                         || name.length() > 2
                            && _is_consonant(prev_char)
                            && _is_consonant(penult_char))
                {
                    // Replace the space with something else if ...
                    // * the name is really short
                    // * we're close to the start/end of the name
                    // * we just got a space
                    // * we're generating a jiyva name, or
                    // * the last two letters were consonants
                    continue;
                }
            }
            else if (name.length() > 1
                     && vowel == prev_char
                     && (vowel == 'y' || vowel == 'i'
                         || random2(5) <= 1))
            {
                // Replace the vowel with something else if the previous
                // letter was the same, and it's a 'y', 'i' or with 2/5 chance.
                continue;
            }

            name += vowel;
        }
        else // We want a consonant.
        {
            // Are we at start or end of the (sub) name?
            const bool beg = (name.empty() || prev_char == ' ');
            const bool end = (name.length() >= len - 2);

            // Use one of number of predefined letter combinations.
            if ((len > 3 || !name.empty())
                && random2(7) <= 1 // 2/7 chance
                && (!beg || !end))
            {
                const int first = (beg ? RCS_BB : (end ? RCS_BE : RCS_BM));
                const int last  = (beg ? RCS_EB : (end ? RCS_EE : RCS_EM));

                const int range = last - first;

                const int cons_seed = random2(range) + first;

                const string consonant_set = _random_consonant_set(cons_seed);

                ASSERT(consonant_set.size() > 1);
                len += consonant_set.size() - 2; // triples increase len
                name += consonant_set;
            }
            else // Place a single letter instead.
            {
                // Pick a random consonant.
                name += _random_cons();
            }
        }

        if (name[name.length() - 1] == ' ')
        {
            ASSERT(name_type != MNAME_JIYVA);
            has_space = true;
        }
    }

    // Catch early exit and try to give a final letter.
    const char last_char = name[name.length() - 1];
    if (!name.empty()
        && last_char != ' '
        && last_char != 'y'
        && !_is_consonant(name[name.length() - 1])
        && (name.length() < len    // early exit
            || (len < 8
                && random2(3) != 0))) // 2/3 chance for other short names
    {
        // Specifically, add a consonant.
        name += _random_cons();
    }

    if (maxlen != SIZE_MAX)
        name = chop_string(name, maxlen);
    trim_string_right(name);

    // Fallback if the name was too short.
    if (name.length() < 4)
    {
        // convolute & recurse
        if (name_type == MNAME_JIYVA)
            return make_name(rng::get_uint32(), MNAME_JIYVA);

        name = "plog";
    }
    name = _unforbid(name);

    string uppercased_name;
    for (size_t i = 0; i < name.length(); i++)
    {
        if (name_type == MNAME_JIYVA)
            ASSERT(name[i] != ' ');

        if (name_type == MNAME_SCROLL || i == 0 || name[i - 1] == ' ')
            uppercased_name += toupper_safe(name[i]);
        else
            uppercased_name += name[i];
    }

    return uppercased_name;
}
#undef ITEMNAME_SIZE

/**
 * Is the given character a lower-case ascii consonant?
 *
 * For our purposes, y is not a consonant.
 */
static bool _is_consonant(char let)
{
    static const set<char> all_consonants = { 'b', 'c', 'd', 'f', 'g',
                                              'h', 'j', 'k', 'l', 'm',
                                              'n', 'p', 'q', 'r', 's',
                                              't', 'v', 'w', 'x', 'z' };
    return all_consonants.count(let);
}

// Returns a random vowel (a, e, i, o, u with equal probability) or space
// or 'y' with lower chances.
static char _random_vowel()
{
    static const char vowels[] = "aeiouaeiouaeiouy  ";
    return vowels[random2(sizeof(vowels) - 1)];
}

// Returns a random consonant with not quite equal probability.
// Does not include 'y'.
static char _random_cons()
{
    static const char consonants[] = "bcdfghjklmnpqrstvwxzcdfghlmnrstlmnrst";
    return consonants[random2(sizeof(consonants) - 1)];
}

/**
 * Choose a random consonant tuple/triple, based on the given seed.
 *
 * @param seed  The index into the consonant array; different seed ranges are
 *              expected to correspond with the place in the name being
 *              generated where the consonants should be inserted.
 * @return      A random length 2 or 3 consonant set; e.g. "kl", "str", etc.
 *              If the seed is out of bounds, return "";
 */
static string _random_consonant_set(size_t c)
{
    // Pick a random combination of consonants from the set below.
    //   begin  -> [RCS_BB, RCS_EB) = [ 0, 27)
    //   middle -> [RCS_BM, RCS_EM) = [ 0, 67)
    //   end    -> [RCS_BE, RCS_EE) = [14, 56)

    static const string consonant_sets[] = {
        // 0-13: start, middle
        "kl", "gr", "cl", "cr", "fr",
        "pr", "tr", "tw", "br", "pl",
        "bl", "str", "shr", "thr",
        // 14-26: start, middle, end
        "sm", "sh", "ch", "th", "ph",
        "pn", "kh", "gh", "mn", "ps",
        "st", "sk", "sch",
        // 27-55: middle, end
        "ts", "cs", "xt", "nt", "ll",
        "rr", "ss", "wk", "wn", "ng",
        "cw", "mp", "ck", "nk", "dd",
        "tt", "bb", "pp", "nn", "mm",
        "kk", "gg", "ff", "pt", "tz",
        "dgh", "rgh", "rph", "rch",
        // 56-66: middle only
        "cz", "xk", "zx", "xz", "cv",
        "vv", "nl", "rh", "dw", "nw",
        "khl",
    };
    COMPILE_CHECK(ARRAYSZ(consonant_sets) == RCS_END);

    ASSERT_RANGE(c, 0, ARRAYSZ(consonant_sets));

    return consonant_sets[c];
}

/**
 * Write all possible scroll names to the given file.
 */
static void _test_scroll_names(const string& fname)
{
    FILE *f = fopen_u(fname.c_str(), "w");
    if (!f)
        sysfail("can't write test output");

    string longest;
    for (int i = 0; i < 151; i++)
    {
        for (int j = 0; j < 151; j++)
        {
            const int seed = i | (j << 8) | (OBJ_SCROLLS << 16);
            const string name = make_name(seed, MNAME_SCROLL);
            if (name.length() > longest.length())
                longest = name;
            fprintf(f, "%s\n", name.c_str());
        }
    }

    fprintf(f, "\nLongest: %s (%d)\n", longest.c_str(), (int)longest.length());

    fclose(f);
}

/**
 * Write one million random Jiyva names to the given file.
 */
static void _test_jiyva_names(const string& fname)
{
    FILE *f = fopen_u(fname.c_str(), "w");
    if (!f)
        sysfail("can't write test output");

    string longest;
    rng::seed(27);
    for (int i = 0; i < 1000000; i++)
    {
        const string name = make_name(rng::get_uint32(), MNAME_JIYVA);
        ASSERT(name[0] == 'J');
        if (name.length() > longest.length())
            longest = name;
        fprintf(f, "%s\n", name.c_str());
    }

    fprintf(f, "\nLongest: %s (%d)\n", longest.c_str(), (int)longest.length());

    fclose(f);
}

/**
 * Test make_name().
 *
 * Currently just a stress test iterating over all possible scroll names.
 */
void make_name_tests()
{
    _test_jiyva_names("jiyva_names.out");
    _test_scroll_names("scroll_names.out");

    rng::seed(27);
    for (int i = 0; i < 1000000; ++i)
        make_name();
}

bool is_interesting_item(const item_def& item)
{
    if (fully_identified(item) && is_artefact(item))
        return true;

    const string iname = item_prefix(item, false) + " " + item.name(DESC_PLAIN);
    for (const text_pattern &pat : Options.note_items)
        if (pat.matches(iname))
            return true;

    return false;
}

/**
 * Is an item a potentially life-saving consumable in emergency situations?
 * Unlike similar functions, this one never takes temporary conditions into
 * account. It does, however, take religion and mutations into account.
 * Permanently unusable items are in general not considered emergency items.
 *
 * @param item The item being queried.
 * @return True if the item is known to be an emergency item.
 */
bool is_emergency_item(const item_def &item)
{
    if (!item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
        case SCR_TELEPORTATION:
        case SCR_BLINKING:
            return !you.stasis();
        case SCR_FEAR:
        case SCR_FOG:
            return true;
        default:
            return false;
        }
    case OBJ_POTIONS:
        if (!you.can_drink())
            return false;

        switch (item.sub_type)
        {
        case POT_HASTE:
            return !have_passive(passive_t::no_haste)
                && !you.stasis();
        case POT_HEAL_WOUNDS:
            return you.can_potion_heal();
        case POT_MAGIC:
            return !you.has_mutation(MUT_HP_CASTING);
        case POT_CURING:
        case POT_RESISTANCE:
            return true;
        default:
            return false;
        }
    case OBJ_MISSILES:
        // Missiles won't help Felids.
        if (you.has_mutation(MUT_NO_GRASPING))
            return false;

        switch (item.sub_type)
        {
        case MI_THROWING_NET:
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

/**
 * Is an item a particularly good consumable? Unlike similar functions,
 * this one never takes temporary conditions into account. Permanently
 * unusable items are in general not considered good.
 *
 * @param item The item being queried.
 * @return True if the item is known to be good.
 */
bool is_good_item(const item_def &item)
{
    if (!item_type_known(item))
        return false;

    if (is_emergency_item(item))
        return true;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        if (item.sub_type == SCR_TORMENT)
            return you.res_torment();
        return item.sub_type == SCR_ACQUIREMENT;
    case OBJ_POTIONS:
        if (!you.can_drink(false)) // still want to pick them up in lichform?
            return false;

        // Recolor healing potions to indicate their additional goodness
        //
        // XX: By default, this doesn't actually change the color of anything
        //     but !ambrosia, since yellow for 'emergency' takes priority over
        //     cyan for 'good'. Should this get a *new* color?
        if (you.has_mutation(MUT_DRUNKEN_BRAWLING)
            && oni_likes_potion(static_cast<potion_type>(item.sub_type)))
        {
            return true;
        }

        switch (item.sub_type)
        {
        case POT_EXPERIENCE:
            return true;
        default:
            return false;
        CASE_REMOVED_POTIONS(item.sub_type)
        }
    default:
        return false;
    }
}

/**
 * Is an item strictly harmful?
 *
 * @param item The item being queried.
 * @return True if the item is known to have only harmful effects.
 */
bool is_bad_item(const item_def &item)
{
    if (!item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_POTIONS:
        // Can't be bad if you can't use them.
        if (!you.can_drink(false))
            return false;

        switch (item.sub_type)
        {
        case POT_DEGENERATION:
            return true;
        default:
            return false;
        CASE_REMOVED_POTIONS(item.sub_type);
        }
    default:
        return false;
    }
}

/**
 * Is an item dangerous but potentially worthwhile?
 *
 * @param item The item being queried.
 * @param temp Should temporary conditions such as transformations and
 *             vampire state be taken into account?  Religion (but
 *             not its absence) is considered to be permanent here.
 * @return True if using the item is known to be risky but occasionally
 *         worthwhile.
 */
bool is_dangerous_item(const item_def &item, bool temp)
{
    // can't assume there is a sensible `you` for various checks here
    if (crawl_state.game_is_arena())
        return false;

    if (!item_type_known(item))
        return false;

    // useless items can hardly be dangerous.
    if (is_useless_item(item, temp))
        return false;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
        case SCR_IMMOLATION:
        case SCR_VULNERABILITY:
        case SCR_NOISE:
            return true;
        case SCR_TORMENT:
            return !you.res_torment();
        case SCR_POISON:
            return player_res_poison(false, temp, true) <= 0
                   && !you.cloud_immune();
        default:
            return false;
        }

    case OBJ_POTIONS:
        switch (item.sub_type)
        {
        case POT_MUTATION:
            if (have_passive(passive_t::cleanse_mut_potions))
                return false;
            // intentional fallthrough
        case POT_LIGNIFY:
        case POT_ATTRACTION:
            return true;
        default:
            return false;
        }

    case OBJ_MISCELLANY:
        // Tremorstones will blow you right up.
        return item.sub_type == MISC_TIN_OF_TREMORSTONES;

    case OBJ_ARMOUR:
        // Tilting at windmills can be dangerous.
        return get_armour_ego_type(item) == SPARM_RAMPAGING;

    default:
        return false;
    }
}

/**
 * If the player has no items matching the given selector, give an appropriate
 * response to print. Otherwise, if they do have such items, return the empty
 * string.
 */
static string _no_items_reason(object_selector type, bool check_floor = false)
{
    if (!any_items_of_type(type, -1, check_floor))
        return no_selectables_message(type);
    return "";
}

static string _general_cannot_read_reason()
{
    // general checks
    if (player_in_branch(BRANCH_GEHENNA))
        return "You cannot see clearly; the smoke and ash is too thick!";

    if (you.berserk())
        return "You are too berserk!";

    if (you.confused())
        return "You are too confused!";

    // no reading while threatened (Ru/random mutation)
    if (you.duration[DUR_NO_SCROLLS] || you.duration[DUR_BRAINLESS])
        return "You cannot read scrolls in your current state!";

    if (silenced(you.pos()))
        return "Magic scrolls do not work when you're silenced!";

    // water elementals
    if (you.duration[DUR_WATER_HOLD] && !you.res_water_drowning())
        return "You cannot read scrolls while unable to breathe!";

    return "";
}

/**
 * If the player is unable to (r)ead the item in the given slot, return the
 * reason why. Otherwise (if they are able to read it), returns "", the empty
 * string. If item is nullptr, do only general reading checks.
 */
string cannot_read_item_reason(const item_def *item, bool temp, bool ident)
{
    // convoluted ordering is because the general checks below need to go before
    // the item id check, but non-temp messages go before general checks
    if (item && item->base_type == OBJ_SCROLLS
        && (ident || item_type_known(*item)))
    {
        // this function handles a few cases of perma-uselessness. For those,
        // be sure to print the message first. (XX generalize)
        switch (item->sub_type)
        {
        case SCR_AMNESIA:
            if (you.has_mutation(MUT_INNATE_CASTER))
                return "You don't have control over your spell memory.";
            // XX possibly amnesia should be allowed to work under Trog, despite
            // being marked useless..
            if (you_worship(GOD_TROG))
                return "Trog doesn't allow you to memorise spells!";
            break;
        case SCR_ENCHANT_WEAPON:
        case SCR_BRAND_WEAPON:
            if (you.has_mutation(MUT_NO_GRASPING))
                return "There's no point in enhancing weapons you can't use!";
            break;
        case SCR_ENCHANT_ARMOUR:
            if (you.has_mutation(MUT_NO_GRASPING))
                return "There's no point in enchanting armour you can't use!";
            break;

        case SCR_IDENTIFY:
            if (you.props.exists(IDENTIFIED_ALL_KEY))
                return "There is nothing left to identify.";
            if (have_passive(passive_t::identify_items))
                return "You have no need of identification.";
            break;

        case SCR_SUMMONING:
        case SCR_BUTTERFLIES:
            if (you.allies_forbidden())
                return "You cannot coerce anything to answer your summons.";
            break;
        case SCR_BLINKING:
        case SCR_TELEPORTATION:
            // XX code duplication with you.no_tele_reason
            if (you.stasis())
                return you.no_tele_reason(item->sub_type == SCR_BLINKING);
            break;
        default:
            break;
        }
    }

    if (temp)
    {
        const string gen = _general_cannot_read_reason();
        if (gen.size())
            return gen;
    }

    if (!item)
        return "";

    // item-specific checks

    // still possible to use * at the `r` prompt. (Why do we allow this now?)
    if (item->base_type != OBJ_SCROLLS)
        return "You can't read that!";

    // temp uselessness only below this check
    if (!temp || (!ident && !item_type_known(*item)))
        return "";

    // don't waste the player's time reading known scrolls in situations where
    // they'd be useless
    switch (item->sub_type)
    {
        case SCR_BLINKING:
        case SCR_TELEPORTATION:
            // note: stasis handled separately above
            return you.no_tele_reason(item->sub_type == SCR_BLINKING);

        case SCR_AMNESIA:
            if (you.spell_no == 0)
                return "You have no spells to forget.";
            return "";

        case SCR_ENCHANT_ARMOUR:
            return _no_items_reason(OSEL_ENCHANTABLE_ARMOUR, true);

        case SCR_ENCHANT_WEAPON:
            return _no_items_reason(OSEL_ENCHANTABLE_WEAPON, true);

        case SCR_BRAND_WEAPON:
            return _no_items_reason(OSEL_BRANDABLE_WEAPON, true);

        case SCR_IDENTIFY:
            return _no_items_reason(OSEL_UNIDENT, true);

        case SCR_FOG:
        case SCR_POISON:
            if (env.level_state & LSTATE_STILL_WINDS)
                return "The air is too still for clouds to form.";
            return "";

        case SCR_REVELATION:
            if (!is_map_persistent())
                return "This place cannot be mapped!";
            return "";

        default:
            return "";
    }
}

string cannot_drink_item_reason(const item_def *item, bool temp,
                                bool use_check, bool ident)
{
    // general permanent reasons
    if (!you.can_drink(false))
        return "You can't drink.";

    const bool valid = item && item->base_type == OBJ_POTIONS
                            && (item_type_known(*item) || ident);
    const potion_type ptyp = valid
        ? static_cast<potion_type>(item->sub_type)
        : NUM_POTIONS;
    string r;

    if (valid)
    {
        // For id'd potions, print temp=false message before temp=true messages
        get_potion_effect(ptyp)->can_quaff(&r, false);
        if (!r.empty())
            return r;
    }

    // general temp reasons
    if (temp)
    {
        if (!you.can_drink(true))
            return "You cannot drink potions in your current state!";

        if (you.berserk())
            return "You are too berserk!";

        if (player_in_branch(BRANCH_COCYTUS))
            return "It's too cold; everything's frozen solid!";
    }

    if (item && item->base_type != OBJ_POTIONS)
        return "You can't drink that!";

    // !valid now means either no item, or unid'd item.
    if (!temp || !valid)
        return "";

    // potion of invis can be used even if temp useless, a warning is printed
    if (use_check && ptyp == POT_INVISIBILITY)
        return "";

    get_potion_effect(ptyp)->can_quaff(&r, true);
    return r;
}

/**
 * Is an item (more or less) useless to the player? Uselessness includes
 * but is not limited to situations such as:
 * \li The item cannot be used.
 * \li Using the item would have no effect.
 * \li Using the item would have purely negative effects (<tt>is_bad_item</tt>).
 * \li Using the item is expected to produce no benefit for a player of their
 *     religious standing. For example, magic enhancers for Trog worshippers
 *     are "useless", even if the player knows a spell and therefore could
 *     benefit.
 *
 * @param item The item being queried.
 * @param temp Should temporary conditions such as transformations and
 *             vampire state be taken into account? Religion (but
 *             not its absence) is considered to be permanent here.
 * @param ident Should uselessness be checked as if the item were already
 *              identified?
 * @return True if the item is known to be useless.
 */
bool is_useless_item(const item_def &item, bool temp, bool ident)
{
    // During game startup, no item is useless. If someone re-glyphs an item
    // based on its uselessness, the glyph-to-item cache will use the useless
    // value even if your god or species can make use of it.
    // similarly for arena: bugs will ensue if the game tries to check any of
    // this
    if (you.species == SP_UNKNOWN // is this really the best way to check this?
        || crawl_state.game_is_arena())
    {
        return false;
    }

    // An ash item that is already being worn and is cursed, counts as useful
    // even if it would otherwise be useless.
    if (will_have_passive(passive_t::bondage_skill_boost)
        && item_is_equipped(item)
        && bool(item.flags & ISFLAG_CURSED))
    {
        if (!temp || !item_is_melded(item))
            return false;
        // if it's melded, just fall through. This might not be accurate in
        // all cases.
    }

    if (temp && you.cannot_act())
        return true;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        return you.has_mutation(MUT_NO_GRASPING)
            || !you.could_wield(item, true, !temp);

    case OBJ_MISSILES:
        // All missiles are useless for felids.
        if (you.has_mutation(MUT_NO_GRASPING))
            return true;

        return !is_throwable(&you, item);

    case OBJ_ARMOUR:
        if (!can_wear_armour(item, false, true))
            return true;

        if (is_shield(item) && you.get_mutation_level(MUT_MISSING_HAND))
            return true;

        if (is_unrandom_artefact(item, UNRAND_WUCAD_MU))
            return you.has_mutation(MUT_HP_CASTING) || you_worship(GOD_TROG);

        if (is_artefact(item))
            return false;

        if (item.sub_type == ARM_SCARF && (ident || item_type_known(item)))
        {
            special_armour_type ego = get_armour_ego_type(item);
            switch (ego)
            {
            case SPARM_SPIRIT_SHIELD:
                return you.spirit_shield(false);
            case SPARM_REPULSION:
                return temp && have_passive(passive_t::upgraded_storm_shield)
                       || you.get_mutation_level(MUT_DISTORTION_FIELD) == 3;
            case SPARM_INVISIBILITY:
                return you.has_mutation(MUT_NO_ARTIFICE)
                       || !invis_allowed(true, nullptr, temp);
            default:
                return false;
            }
        }
        if (item.sub_type == ARM_ORB && (ident || item_type_known(item)))
        {
            special_armour_type ego = get_armour_ego_type(item);
            switch (ego)
            {
            case SPARM_RAGE:
                return !you.can_go_berserk(false, false, true, nullptr, temp);
            case SPARM_ENERGY:
                return you.has_mutation(MUT_HP_CASTING) || you_worship(GOD_TROG);
            default:
                return false;
            }
        }
        return false;

    case OBJ_SCROLLS:
    {
        // general reasons: player is berserk, in gehenna, drowning, etc.
        // even unid'd items count as useless under these circumstances
        if (cannot_read_item_reason(nullptr, temp).size())
            return true;

        // otherwise, unid'd items can always be read
        if (!ident && !item_type_known(item))
            return false;

        // An (id'd) bad item is always useless.
        if (is_bad_item(item))
            return true;

        const string reasons = cannot_read_item_reason(&item, temp, ident);
        return reasons.size();
    }

    case OBJ_TALISMANS:
    case OBJ_MISCELLANY:
    case OBJ_WANDS:
        return cannot_evoke_item_reason(&item, temp, ident || item_type_known(item)).size();

    case OBJ_POTIONS:
    {
        // general reasons: player is a mummy, player in cocytus, etc.
        if (cannot_drink_item_reason(nullptr, temp).size())
            return true;

        // always allow drinking unid'd potions if the player can drink at all
        if (!ident && !item_type_known(item))
            return false;

        // A bad item is always useless.
        if (is_bad_item(item))
            return true;

        // specific reasons
        return cannot_drink_item_reason(&item, temp, false, ident).size();
    }
    case OBJ_JEWELLERY:
        if (temp && bool(!you_can_wear(get_item_slot(item))))
            return true;

        if (you.has_mutation(MUT_NO_JEWELLERY))
            return true;

        if (!ident && !item_type_known(item))
            return false;

        // Potentially useful. TODO: check the properties.
        if (is_artefact(item))
            return false;

        if (is_bad_item(item))
            return true;

        switch (item.sub_type)
        {
        case RING_RESIST_CORROSION:
            return you.res_corr(false, false);

        case AMU_ACROBAT:
            return you.has_mutation(MUT_ACROBATIC);

        case AMU_FAITH:
            return (you.has_mutation(MUT_FORLORN) && !you.religion) // ??
                    || you.has_mutation(MUT_FAITH)
                    || !ignore_faith_reason().empty();

        case AMU_GUARDIAN_SPIRIT:
            return you.spirit_shield(false) || you.has_mutation(MUT_HP_CASTING);

        case RING_POSITIVE_ENERGY:
            return player_prot_life(false, temp, false) == 3;

        case AMU_REGENERATION:
            return
#if TAG_MAJOR_VERSION == 34
                   you.get_mutation_level(MUT_NO_REGENERATION) > 0 ||
#endif
                     (temp
                       && (you.get_mutation_level(MUT_INHIBITED_REGENERATION) > 0
                           || you.has_mutation(MUT_VAMPIRISM))
                       && regeneration_is_inhibited());

        case AMU_MANA_REGENERATION:
            return !you.max_magic_points;

        case RING_MAGICAL_POWER:
            return you.has_mutation(MUT_HP_CASTING);

        case RING_SEE_INVISIBLE:
            return you.innate_sinv();

        case RING_POISON_RESISTANCE:
            return player_res_poison(false, temp, false) > 0;

        case RING_WIZARDRY:
            return you_worship(GOD_TROG);

        case RING_FLIGHT:
            return you.permanent_flight(false);

        case RING_STEALTH:
            return you.get_mutation_level(MUT_NO_STEALTH);

        default:
            return false;
        }

#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
            return true;
#endif

    case OBJ_STAVES:
        if (you.has_mutation(MUT_NO_GRASPING))
            return true;
        if (!you.could_wield(item, true, !temp))
        {
            // Weapon is too large (or small) to be wielded and cannot
            // be thrown either.
            return true;
        }
        if (!ident && !item_type_known(item))
            return false;

        return false;

    case OBJ_CORPSES:
        return true;

    case OBJ_BOOKS:
        if (you.has_mutation(MUT_INNATE_CASTER) && item.sub_type != BOOK_MANUAL)
            return true;
        if (item.sub_type == NUM_BOOKS)
            return false;
        if (item.sub_type != BOOK_MANUAL)
        {
            // Spellbooks are useless if all spells are either in the library
            // already or are uncastable.
            bool useless = true;
            for (spell_type st : spells_in_book(item))
                if (!you.spell_library[st] && you_can_memorise(st))
                    useless = false;
            return useless;
        }
        // If we're here, it's a manual.
        if (you.skills[item.plus] >= 27)
            return true;
        return is_useless_skill((skill_type)item.plus);

    default:
        return false;
    }
    return false;
}

string item_prefix(const item_def &item, bool temp)
{
    vector<const char *> prefixes;

    if (!item.defined())
        return "";

    if (fully_identified(item))
        prefixes.push_back("identified");
    else if (item_ident(item, ISFLAG_KNOW_TYPE)
             || get_ident_type(item))
    {
        prefixes.push_back("known");
    }
    else
        prefixes.push_back("unidentified");

    if (god_hates_item(item))
    {
        prefixes.push_back("evil_item");
        prefixes.push_back("forbidden");
    }

    if (is_emergency_item(item))
        prefixes.push_back("emergency_item");
    if (is_good_item(item))
        prefixes.push_back("good_item");
    if (is_dangerous_item(item, temp))
        prefixes.push_back("dangerous_item");
    if (is_bad_item(item))
        prefixes.push_back("bad_item");
    if (is_useless_item(item, temp))
        prefixes.push_back("useless_item");

    if (item_is_stationary(item))
        prefixes.push_back("stationary");

    if (!is_artefact(item) && (item.base_type == OBJ_WEAPONS
                               || item.base_type == OBJ_ARMOUR))
    {
        if (item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus > 0)
            prefixes.push_back("enchanted");
        if (item_ident(item, ISFLAG_KNOW_TYPE) && item.brand)
            prefixes.push_back("ego");
    }

    switch (item.base_type)
    {
    case OBJ_STAVES:
    case OBJ_WEAPONS:
        if (is_range_weapon(item))
            prefixes.push_back("ranged");
        else if (is_melee_weapon(item)) // currently redundant
            prefixes.push_back("melee");
        // fall through

    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
    case OBJ_TALISMANS:
        if (is_unrandom_artefact(item))
            prefixes.push_back("unrand");
        if (is_artefact(item))
            prefixes.push_back("artefact");
        // fall through

    case OBJ_MISSILES:
        if (item_is_equipped(item, true))
            prefixes.push_back("equipped");
        break;

    case OBJ_BOOKS:
        if (item.sub_type != BOOK_MANUAL && item.sub_type != NUM_BOOKS)
            prefixes.push_back("spellbook");
        break;

    case OBJ_MISCELLANY:
        if (is_xp_evoker(item))
            prefixes.push_back("evoker");
        break;

    default:
        break;
    }

    prefixes.push_back(item_class_name(item.base_type, true));

    string result = comma_separated_line(prefixes.begin(), prefixes.end(),
                                         " ", " ");

    return result;
}

/**
 * Return an item's name surrounded by colour tags, using menu colouring
 *
 * @param item The item being queried
 * @param desc The description level to use for the name string
 * @return A string containing the item's name surrounded by colour tags
 */
string menu_colour_item_name(const item_def &item, description_level_type desc)
{
    const string cprf      = item_prefix(item, false);
    const string item_name = item.name(desc);

    const int col = menu_colour(item_name, cprf, "pickup", false);
    if (col == -1)
        return item_name;

    const string colour = colour_to_str(col);
    const char * const colour_z = colour.c_str();
    return make_stringf("<%s>%s</%s>", colour_z, item_name.c_str(), colour_z);
}

typedef map<string, item_kind> item_names_map;
static item_names_map item_names_cache;

typedef map<unsigned, vector<string> > item_names_by_glyph_map;
static item_names_by_glyph_map item_names_by_glyph_cache;

void init_item_name_cache()
{
    item_names_cache.clear();
    item_names_by_glyph_cache.clear();

    for (int i = 0; i < NUM_OBJECT_CLASSES; i++)
    {
        const object_class_type base_type = static_cast<object_class_type>(i);

        for (const auto sub_type : all_item_subtypes(base_type))
        {
            if (base_type == OBJ_BOOKS)
            {
                if (sub_type == BOOK_RANDART_LEVEL
                    || sub_type == BOOK_RANDART_THEME)
                {
                    // These are randart only and have no fixed names.
                    continue;
                }
            }

            int npluses = 0;
            // this iterates through all skills for manuals, caching the
            // resulting names. Weird.
            if (base_type == OBJ_BOOKS && sub_type == BOOK_MANUAL)
                npluses = NUM_SKILLS;

            item_def item;
            item.base_type = base_type;
            item.sub_type = sub_type;
            for (int plus = 0; plus <= npluses; plus++)
            {
                // strange logic: this seems to be designed to put both "Manual"
                // and "Manual of fighting" in the cache for item.plus == 0
                if (plus > 0)
                    item.plus = max(0, plus - 1);
                string name = item.name(plus || item.base_type == OBJ_RUNES ? DESC_PLAIN : DESC_DBNAME,
                                        true, true);
                lowercase(name);
                cglyph_t g = get_item_glyph(item);

                if (base_type == OBJ_JEWELLERY && sub_type >= NUM_RINGS
                    && sub_type < AMU_FIRST_AMULET)
                {
                    continue;
                }
                else if (name.find("buggy") != string::npos)
                {
                    mprf(MSGCH_ERROR, "Bad name for item name cache: %s",
                                                                name.c_str());
                    continue;
                }
                const bool removed = item_type_removed(base_type, sub_type)
                    || base_type == OBJ_BOOKS && sub_type == BOOK_MANUAL
                        && is_removed_skill(static_cast<skill_type>(item.plus));

                if (!item_names_cache.count(name))
                {
                    // what would happen if we don't put removed items in the
                    // item name cache?
                    item_names_cache[name] = { base_type, (uint8_t)sub_type,
                                               (int8_t)item.plus, 0 };

                    // only used for help lookup, skip removed items
                    if (g.ch && !removed)
                        item_names_by_glyph_cache[g.ch].push_back(name);
                }
            }
        }
    }

    ASSERT(!item_names_cache.empty());
}

item_kind item_kind_by_name(const string &name)
{
    return lookup(item_names_cache, lowercase_string(name),
                  { OBJ_UNASSIGNED, 0, 0, 0 });
}

vector<string> item_name_list_for_glyph(char32_t glyph)
{
    return lookup(item_names_by_glyph_cache, glyph, {});
}

bool is_named_corpse(const item_def &corpse)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);

    return corpse.props.exists(CORPSE_NAME_KEY);
}

string get_corpse_name(const item_def &corpse, monster_flags_t *name_type)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);

    if (!corpse.props.exists(CORPSE_NAME_KEY))
        return "";

    if (name_type != nullptr)
        name_type->flags = corpse.props[CORPSE_NAME_TYPE_KEY].get_int64();

    return corpse.props[CORPSE_NAME_KEY].get_string();
}

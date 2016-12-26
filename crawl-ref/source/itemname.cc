/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "itemname.h"

#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "pcg.h" // for make_name()'s use
#include "branch.h"
#include "butcher.h"
#include "cio.h"
#include "colour.h"
#include "command.h"
#include "decks.h"
#include "describe.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h" // LSTATE_STILL_WINDS
#include "errors.h" // sysfail
#include "evoke.h"
#include "food.h"
#include "goditem.h"
#include "godpassive.h" // passive_t::want_curses, no_haste
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "makeitem.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "place.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "throw.h"
#include "transform.h"
#include "unicode.h"
#include "unwind.h"
#include "viewgeom.h"

static bool _is_consonant(char let);
static char _random_vowel(int seed);
static char _random_cons(int seed);
static string _random_consonant_set(int seed);

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
        && item.is_type(OBJ_ARMOUR, ARM_SHIELD))
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
        if (Options.show_god_gift == MB_TRUE
            || Options.show_god_gift == MB_MAYBE && !fully_identified(item))
        {
            insparts.push_back(orig);
        }
    }

    if (is_artefact(item))
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
    {
        ignore_flags |= ISFLAG_KNOW_PLUSES | ISFLAG_KNOW_CURSE
                        | ISFLAG_COSMETIC_MASK;
    }

    if (descrip == DESC_NONE)
        return "";

    ostringstream buff;

    const string auxname = name_aux(descrip, terse, ident, with_inscription,
                                    ignore_flags);

    const bool startvowel     = is_vowel(auxname[0]);

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

    if (base_type == OBJ_BOOKS && (ident || item_type_known(*this))
        && book_has_title(*this))
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
        || item_is_orb(*this) || item_is_horn_of_geryon(*this)
        || (ident || item_type_known(*this)) && is_artefact(*this)
            && special != UNRAND_OCTOPUS_KING_RING)
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
                    else if (you.species == SP_FELID)
                        buff << " (in mouth)";
                    else
                        buff << " (in " << you.hand_name(false) << ")";
                    break;
                case EQ_CLOAK:
                case EQ_HELMET:
                case EQ_GLOVES:
                case EQ_BOOTS:
                case EQ_SHIELD:
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
                default:
                    die("Item in an invalid slot");
                }
            }
        }
        else if (item_is_quivered(*this))
            buff << " (quivered)";
    }

    if (descrip != DESC_BASENAME && descrip != DESC_DBNAME && with_inscription)
        buff << _item_inscription(*this);

    // These didn't have "cursed " prepended; add them here so that
    // it comes after the inscription.
    if (terse && descrip != DESC_DBNAME && descrip != DESC_BASENAME
        && descrip != DESC_QUALNAME
        && is_artefact(*this) && cursed()
        && !testbits(ignore_flags, ISFLAG_KNOW_CURSE)
        && (ident || item_ident(*this, ISFLAG_KNOW_CURSE)))
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
    case SPMSL_EXPLODING:
    case SPMSL_STEEL:
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
    case SPMSL_EXPLODING:
        return t == MBN_TERSE ? "explode" : "exploding";
    case SPMSL_STEEL:
        return "steel";
    case SPMSL_SILVER:
        return "silver";
    case SPMSL_PARALYSIS:
        return "paralysis";
#if TAG_MAJOR_VERSION == 34
    case SPMSL_SLOW:
        return t == MBN_TERSE ? "slow" : "slowing";
#endif
    case SPMSL_SLEEP:
        return t == MBN_TERSE ? "sleep" : "sleeping";
    case SPMSL_CONFUSION:
        return t == MBN_TERSE ? "conf" : "confusion";
#if TAG_MAJOR_VERSION == 34
    case SPMSL_SICKNESS:
        return t == MBN_TERSE ? "sick" : "sickness";
#endif
    case SPMSL_FRENZY:
        return "frenzy";
    case SPMSL_RETURNING:
        return t == MBN_TERSE ? "return" : "returning";
    case SPMSL_CHAOS:
        return "chaos";
    case SPMSL_PENETRATION:
        return t == MBN_TERSE ? "penet" : "penetration";
    case SPMSL_DISPERSAL:
        return t == MBN_TERSE ? "disperse" : "dispersal";
#if TAG_MAJOR_VERSION == 34
    case SPMSL_BLINDING:
        return t == MBN_TERSE ? "blind" : "blinding";
#endif
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
    "venom", "protect", "drain", "speed", "buggy-vorpal",
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
    "penet", "reap", "buggy-num", "acid",
#if TAG_MAJOR_VERSION > 34
    "confuse",
#endif
    "debug",
};

static const char *weapon_brands_verbose[] =
{
    "", "flaming", "freezing", "holy wrath", "electrocution",
#if TAG_MAJOR_VERSION == 34
    "orc slaying", "dragon slaying",
#endif
    "venom", "protection", "draining", "speed", "buggy-vorpal",
#if TAG_MAJOR_VERSION == 34
    "flame", "frost",
#endif
    "", "pain", "", "distortion",
#if TAG_MAJOR_VERSION == 34
    "reaching", "returning",
#endif
    "chaos",
#if TAG_MAJOR_VERSION == 34
    "evasion", "confusion",
#endif
    "penetration", "reaping", "buggy-num", "acid",
#if TAG_MAJOR_VERSION > 34
    "confusion",
#endif
    "debug",
};

/**
 * What's the name of a type of vorpal brand?
 *
 * @param item      The weapon with the vorpal brand.
 * @param bool      Whether to use a terse or verbose name.
 * @return          The name of the given item's brand.
 */
static const char* _vorpal_brand_name(const item_def &item, bool terse)
{
    // Dummy "All Hand Weapons" item from objstat.
    if (item.sub_type == NUM_WEAPONS)
        return "vorpal";

    if (is_range_weapon(item))
        return "velocity";

    // Would be nice to implement this as an array (like other brands), but
    // mapping the DVORP flags to array entries seems very fragile.
    switch (get_vorpal_type(item))
    {
        case DVORP_CRUSHING: return terse ? "crush" :"crushing";
        case DVORP_SLICING:  return terse ? "slice" : "slicing";
        case DVORP_PIERCING: return terse ? "pierce" : "piercing";
        case DVORP_CHOPPING: return terse ? "chop" : "chopping";
        case DVORP_SLASHING: return terse ? "slash" :"slashing";
        default:             return terse ? "buggy vorpal"
                                          : "buggy destruction";
    }
}


/**
 * What's the name of a weapon brand brand?
 *
 * @param brand             The type of brand in question.
 * @param bool              Whether to use a terse or verbose name.
 * @return                  The name of the given brand.
 */
const char* brand_type_name(int brand, bool terse)
{
    COMPILE_CHECK(ARRAYSZ(weapon_brands_terse) == NUM_SPECIAL_WEAPONS);
    COMPILE_CHECK(ARRAYSZ(weapon_brands_verbose) == NUM_SPECIAL_WEAPONS);

    if (brand < 0 || brand >= NUM_SPECIAL_WEAPONS)
        return terse ? "buggy" : "bugginess";

    return (terse ? weapon_brands_terse : weapon_brands_verbose)[brand];
}

/**
 * What's the name of a given weapon's brand?
 *
 * @param item              The weapon with the brand.
 * @param bool              Whether to use a terse or verbose name.
 * @param override_brand    A brand type to use, instead of the weapon's actual
 *                          brand.
 * @return                  The name of the given item's brand.
 */
const char* weapon_brand_name(const item_def& item, bool terse,
                              int override_brand)
{
    const int brand = override_brand ? override_brand : get_weapon_brand(item);

    if (brand == SPWPN_VORPAL)
        return _vorpal_brand_name(item, terse);

    return brand_type_name(brand, terse);
}

const char* armour_ego_name(const item_def& item, bool terse)
{
    if (!terse)
    {
        switch (get_armour_ego_type(item))
        {
        case SPARM_NORMAL:            return "";
        case SPARM_RUNNING:
            // "naga barding of running" doesn't make any sense, and yes,
            // they are possible. The terse ego name for these is {run}
            // still to avoid player confusion, it used to be {sslith}.
            if (item.sub_type == ARM_NAGA_BARDING)
                                      return "speedy slithering";
            else
                                      return "running";
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

        case SPARM_MAGIC_RESISTANCE:  return "magic resistance";
        case SPARM_PROTECTION:        return "protection";
        case SPARM_STEALTH:           return "stealth";
        case SPARM_RESISTANCE:        return "resistance";
        case SPARM_POSITIVE_ENERGY:   return "positive energy";
        case SPARM_ARCHMAGI:          return "the Archmagi";
#if TAG_MAJOR_VERSION == 34
        case SPARM_JUMPING:           return "jumping";
        case SPARM_PRESERVATION:      return "preservation";
#endif
        case SPARM_REFLECTION:        return "reflection";
        case SPARM_SPIRIT_SHIELD:     return "spirit shield";
        case SPARM_ARCHERY:           return "archery";
        default:                      return "bugginess";
        }
    }
    else
    {
        switch (get_armour_ego_type(item))
        {
        case SPARM_NORMAL:            return "";
        case SPARM_RUNNING:           return "run";
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
        case SPARM_MAGIC_RESISTANCE:  return "MR+";
        case SPARM_PROTECTION:        return "AC+3";
        case SPARM_STEALTH:           return "Stlth+";
        case SPARM_RESISTANCE:        return "rC+ rF+";
        case SPARM_POSITIVE_ENERGY:   return "rN+";
        case SPARM_ARCHMAGI:          return "Archmagi";
#if TAG_MAJOR_VERSION == 34
        case SPARM_JUMPING:           return "obsolete";
        case SPARM_PRESERVATION:      return "obsolete";
#endif
        case SPARM_REFLECTION:        return "reflect";
        case SPARM_SPIRIT_SHIELD:     return "Spirit";
        case SPARM_ARCHERY:           return "archery";
        default:                      return "buggy";
        }
    }
}

static const char* _wand_type_name(int wandtype)
{
    switch (wandtype)
    {
    case WAND_FLAME:           return "flame";
    case WAND_SLOWING:         return "slowing";
    case WAND_PARALYSIS:       return "paralysis";
    case WAND_CONFUSION:       return "confusion";
    case WAND_DIGGING:         return "digging";
    case WAND_ICEBLAST:        return "iceblast";
    case WAND_LIGHTNING:       return "lightning";
    case WAND_POLYMORPH:       return "polymorph";
    case WAND_ENSLAVEMENT:     return "enslavement";
    case WAND_ACID:            return "acid";
    case WAND_RANDOM_EFFECTS:  return "random effects";
    case WAND_DISINTEGRATION:  return "disintegration";
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
    case POT_AGILITY:           return "agility";
    case POT_BRILLIANCE:        return "brilliance";
#if TAG_MAJOR_VERSION == 34
    case POT_GAIN_STRENGTH:     return "gain strength";
    case POT_GAIN_DEXTERITY:    return "gain dexterity";
    case POT_GAIN_INTELLIGENCE: return "gain intelligence";
    case POT_STRONG_POISON:     return "strong poison";
    case POT_PORRIDGE:          return "porridge";
    case POT_SLOWING:           return "slowing";
#endif
    case POT_FLIGHT:            return "flight";
#if TAG_MAJOR_VERSION == 34
    case POT_POISON:            return "poison";
#endif
    case POT_CANCELLATION:      return "cancellation";
    case POT_AMBROSIA:          return "ambrosia";
    case POT_INVISIBILITY:      return "invisibility";
    case POT_DEGENERATION:      return "degeneration";
#if TAG_MAJOR_VERSION == 34
    case POT_DECAY:             return "decay";
#endif
    case POT_EXPERIENCE:        return "experience";
    case POT_MAGIC:             return "magic";
#if TAG_MAJOR_VERSION == 34
    case POT_RESTORE_ABILITIES: return "restore abilities";
#endif
    case POT_BERSERK_RAGE:      return "berserk rage";
    case POT_CURE_MUTATION:     return "cure mutation";
    case POT_MUTATION:          return "mutation";
    case POT_BLOOD:             return "blood";
#if TAG_MAJOR_VERSION == 34
    case POT_BLOOD_COAGULATED:  return "coagulated blood";
#endif
    case POT_RESISTANCE:        return "resistance";
    case POT_LIGNIFY:           return "lignification";
    case POT_BENEFICIAL_MUTATION: return "beneficial mutation";
    default:                    return "bugginess";
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
    case SCR_REMOVE_CURSE:       return "remove curse";
    case SCR_SUMMONING:          return "summoning";
    case SCR_ENCHANT_WEAPON:     return "enchant weapon";
    case SCR_ENCHANT_ARMOUR:     return "enchant armour";
    case SCR_TORMENT:            return "torment";
    case SCR_RANDOM_USELESSNESS: return "random uselessness";
#if TAG_MAJOR_VERSION == 34
    case SCR_CURSE_WEAPON:       return "curse weapon";
    case SCR_CURSE_ARMOUR:       return "curse armour";
    case SCR_CURSE_JEWELLERY:    return "curse jewellery";
#endif
    case SCR_IMMOLATION:         return "immolation";
    case SCR_BLINKING:           return "blinking";
    case SCR_MAGIC_MAPPING:      return "magic mapping";
    case SCR_FOG:                return "fog";
    case SCR_ACQUIREMENT:        return "acquirement";
    case SCR_BRAND_WEAPON:       return "brand weapon";
    case SCR_RECHARGING:         return "recharging";
    case SCR_HOLY_WORD:          return "holy word";
    case SCR_VULNERABILITY:      return "vulnerability";
    case SCR_SILENCE:            return "silence";
    case SCR_AMNESIA:            return "amnesia";
    default:                     return "bugginess";
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
#endif
        case RING_PROTECTION:            return "protection";
        case RING_PROTECTION_FROM_FIRE:  return "protection from fire";
        case RING_POISON_RESISTANCE:     return "poison resistance";
        case RING_PROTECTION_FROM_COLD:  return "protection from cold";
        case RING_STRENGTH:              return "strength";
        case RING_SLAYING:               return "slaying";
        case RING_SEE_INVISIBLE:         return "see invisible";
        case RING_RESIST_CORROSION:      return "resist corrosion";
        case RING_LOUDNESS:              return "loudness";
        case RING_TELEPORTATION:         return "teleportation";
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
        case RING_LIFE_PROTECTION:       return "positive energy";
        case RING_PROTECTION_FROM_MAGIC: return "protection from magic";
        case RING_FIRE:                  return "fire";
        case RING_ICE:                   return "ice";
#if TAG_MAJOR_VERSION == 34
        case RING_TELEPORT_CONTROL:      return "teleport control";
#endif
        case AMU_RAGE:              return "rage";
        case AMU_HARM:              return "harm";
        case AMU_MANA_REGENERATION: return "magic regeneration";
        case AMU_THE_GOURMAND:      return "gourmand";
#if TAG_MAJOR_VERSION == 34
        case AMU_DISMISSAL:         return "obsoleteness";
        case AMU_CONSERVATION:      return "conservation";
        case AMU_CONTROLLED_FLIGHT: return "controlled flight";
#endif
        case AMU_INACCURACY:        return "inaccuracy";
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
#endif
        case RING_PROTECTION:            return "AC";
        case RING_PROTECTION_FROM_FIRE:  return "rF+";
        case RING_POISON_RESISTANCE:     return "rPois";
        case RING_PROTECTION_FROM_COLD:  return "rC+";
        case RING_STRENGTH:              return "Str";
        case RING_SLAYING:               return "Slay";
        case RING_SEE_INVISIBLE:         return "sInv";
        case RING_RESIST_CORROSION:      return "rCorr";
        case RING_LOUDNESS:              return "Stlth-";
        case RING_EVASION:               return "EV";
        case RING_STEALTH:               return "Stlth+";
        case RING_DEXTERITY:             return "Dex";
        case RING_INTELLIGENCE:          return "Int";
        case RING_MAGICAL_POWER:         return "MP+9";
        case RING_FLIGHT:                return "+Fly";
        case RING_LIFE_PROTECTION:       return "rN+";
        case RING_PROTECTION_FROM_MAGIC: return "MR+";
        case AMU_RAGE:                   return "+Rage";
        case AMU_REGENERATION:           return "Regen";
        case AMU_REFLECTION:             return "Reflect";
        case AMU_NOTHING:                return "";
        default: return "buggy";
        }
    }
}

// lua doesn't want "the" in gourmand, but we do, so...
static const char* _jewellery_effect_prefix(int jeweltype)
{
    switch (static_cast<jewellery_type>(jeweltype))
    {
    case AMU_THE_GOURMAND: return "the ";
    default:               return "";
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
    return make_stringf("%s %s%s", _jewellery_class_name(jeweltype),
                                   _jewellery_effect_prefix(jeweltype),
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

const char* deck_rarity_name(deck_rarity_type rarity)
{
    switch (rarity)
    {
    case DECK_RARITY_COMMON:    return "plain";
    case DECK_RARITY_RARE:      return "ornate";
    case DECK_RARITY_LEGENDARY: return "legendary";
    default:                    return "buggy rarity";
    }
}

static string misc_type_name(int type, bool known)
{
    if (is_deck_type(type, true))
    {
        if (!known)
            return "deck of cards";
        return deck_name(type);
    }

    switch (static_cast<misc_item_type>(type))
    {
    case MISC_CRYSTAL_BALL_OF_ENERGY:    return "crystal ball of energy";
    case MISC_BOX_OF_BEASTS:             return "box of beasts";
#if TAG_MAJOR_VERSION == 34
    case MISC_BUGGY_EBONY_CASKET:        return "removed ebony casket";
#endif
    case MISC_FAN_OF_GALES:              return "fan of gales";
    case MISC_LAMP_OF_FIRE:              return "lamp of fire";
#if TAG_MAJOR_VERSION == 34
    case MISC_BUGGY_LANTERN_OF_SHADOWS:  return "removed lantern of shadows";
#endif
    case MISC_HORN_OF_GERYON:            return "horn of Geryon";
    case MISC_DISC_OF_STORMS:            return "disc of storms";
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

    default:
        return "buggy miscellaneous item";
    }
}

static bool _book_visually_special(uint32_t s)
{
    return s & 128; // one in ten books; c.f. item_colour()
}

static const char* book_secondary_string(uint32_t s)
{
    if (!_book_visually_special(s))
        return "";

    static const char* const secondary_strings[] = {
        "", "chunky ", "thick ", "thin ", "wide ", "glowing ",
        "dog-eared ", "oblong ", "runed ", "", "", ""
    };
    return secondary_strings[(s / NDSC_BOOK_PRI) % ARRAYSZ(secondary_strings)];
}

static const char* book_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        "paperback", "hardcover", "leatherbound", "metal-bound", "papyrus",
    };
    COMPILE_CHECK(NDSC_BOOK_PRI == ARRAYSZ(primary_strings));

    return primary_strings[p % ARRAYSZ(primary_strings)];
}

static const char* _book_type_name(int booktype)
{
    switch (static_cast<book_type>(booktype))
    {
    case BOOK_MINOR_MAGIC:            return "Minor Magic";
    case BOOK_CONJURATIONS:           return "Conjurations";
    case BOOK_FLAMES:                 return "Flames";
    case BOOK_FROST:                  return "Frost";
    case BOOK_SUMMONINGS:             return "Summonings";
    case BOOK_FIRE:                   return "Fire";
    case BOOK_ICE:                    return "Ice";
    case BOOK_SPATIAL_TRANSLOCATIONS: return "Spatial Translocations";
    case BOOK_ENCHANTMENTS:           return "Enchantments";
    case BOOK_TEMPESTS:               return "the Tempests";
    case BOOK_DEATH:                  return "Death";
    case BOOK_MISFORTUNE:             return "Misfortune";
    case BOOK_CHANGES:                return "Changes";
    case BOOK_TRANSFIGURATIONS:       return "Transfigurations";
    case BOOK_BATTLE:                 return "Battle";
    case BOOK_CLOUDS:                 return "Clouds";
    case BOOK_NECROMANCY:             return "Necromancy";
    case BOOK_CALLINGS:               return "Callings";
    case BOOK_MALEDICT:               return "Maledictions";
    case BOOK_AIR:                    return "Air";
    case BOOK_SKY:                    return "the Sky";
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
    case BOOK_ALCHEMY:                return "Alchemy";
    case BOOK_BEASTS:                 return "Beasts";
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

static const char* staff_type_name(int stafftype)
{
    switch ((stave_type)stafftype)
    {
    case STAFF_WIZARDRY:    return "wizardry";
    case STAFF_POWER:       return "power";
    case STAFF_FIRE:        return "fire";
    case STAFF_COLD:        return "cold";
    case STAFF_POISON:      return "poison";
    case STAFF_ENERGY:      return "energy";
    case STAFF_DEATH:       return "death";
    case STAFF_CONJURATION: return "conjuration";
#if TAG_MAJOR_VERSION == 34
    case STAFF_ENCHANTMENT: return "enchantment";
#endif
    case STAFF_AIR:         return "air";
    case STAFF_EARTH:       return "earth";
    case STAFF_SUMMONING:   return "summoning";
    default:                return "bugginess";
    }
}

static const char* rod_type_name(int type)
{
    switch ((rod_type)type)
    {
#if TAG_MAJOR_VERSION == 34
    case ROD_SWARM:           return "the swarm";
    case ROD_WARDING:         return "warding";
#endif
    case ROD_LIGHTNING:       return "lightning";
    case ROD_IRON:            return "iron";
    case ROD_SHADOWS:         return "shadows";
#if TAG_MAJOR_VERSION == 34
    case ROD_VENOM:           return "venom";
#endif
    case ROD_INACCURACY:      return "inaccuracy";

    case ROD_IGNITION:        return "ignition";
    case ROD_CLOUDS:          return "clouds";
#if TAG_MAJOR_VERSION == 34
    case ROD_DESTRUCTION:     return "destruction";
#endif

    default: return "bugginess";
    }
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
    case OBJ_FOOD: return "food";
    case OBJ_SCROLLS: return "scroll";
    case OBJ_JEWELLERY: return "jewellery";
    case OBJ_POTIONS: return "potion";
    case OBJ_BOOKS: return "book";
    case OBJ_STAVES: return "staff";
    case OBJ_RODS: return "rod";
    case OBJ_ORBS: return "orb";
    case OBJ_MISCELLANY: return "miscellaneous";
    case OBJ_CORPSES: return "corpse";
    case OBJ_GOLD: return "gold";
    case OBJ_RUNES: return "rune";
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
    case OBJ_MISSILES: // variable to itemprop.cc.
    case OBJ_ARMOUR:
        return item_base_name(type, sub_type);
    case OBJ_WANDS: return _wand_type_name(sub_type);
    case OBJ_FOOD: return food_type_name(sub_type);
    case OBJ_SCROLLS: return scroll_type_name(sub_type);
    case OBJ_JEWELLERY: return jewellery_type_name(sub_type);
    case OBJ_POTIONS: return potion_type_name(sub_type);
    case OBJ_BOOKS:
    {
        if (sub_type == BOOK_MANUAL)
        {
            if (!known)
                return "manual";
            string bookname = "manual of ";
            bookname += skill_name(static_cast<skill_type>(item.plus));
            return bookname;
        }
        else if (sub_type == BOOK_NECRONOMICON)
            return "Necronomicon";
        else if (sub_type == BOOK_GRAND_GRIMOIRE)
            return "Grand Grimoire";
#if TAG_MAJOR_VERSION == 34
        else if (sub_type == BOOK_BUGGY_DESTRUCTION)
            return "tome of obsoleteness";
#endif
        else if (sub_type == BOOK_YOUNG_POISONERS)
            return "Young Poisoner's Handbook";
        else if (sub_type == BOOK_FEN)
            return "Fen Folio";
#if TAG_MAJOR_VERSION == 34
        else if (sub_type == BOOK_AKASHIC_RECORD)
            return "Akashic Record";
#endif

        return string("book of ") + _book_type_name(sub_type);
    }
    case OBJ_STAVES: return staff_type_name(static_cast<stave_type>(sub_type));
    case OBJ_RODS:   return rod_type_name(static_cast<rod_type>(sub_type));
    case OBJ_MISCELLANY: return misc_type_name(sub_type, known);
    // these repeat as base_type_string
    case OBJ_ORBS: return "orb of Zot";
    case OBJ_CORPSES: return "corpse";
    case OBJ_GOLD: return "gold";
    case OBJ_RUNES: return "rune of Zot";
    default: return "";
    }
}

/**
 * What's the name for the weapon used by a given ghost?
 *
 * There's no actual weapon info, just brand, so we have to improvise...
 *
 * @param brand     The brand_type used by the ghost.
 * @return          The name of the ghost's weapon (e.g. "a weapon of flaming",
 *                  "an antimagic weapon")
 */
string ghost_brand_name(int brand)
{
    // XXX: deduplicate these special cases
    if (brand == SPWPN_VAMPIRISM)
        return "a vampiric weapon";
    if (brand == SPWPN_ANTIMAGIC)
        return "an antimagic weapon";
    if (brand == SPWPN_VORPAL)
        return "a vorpal weapon"; // can't use brand_type_name
    return make_stringf("a weapon of %s", brand_type_name(brand, false));
}

string ego_type_string(const item_def &item, bool terse, int override_brand)
{
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return armour_ego_name(item, terse);
    case OBJ_WEAPONS:
        if (!terse)
        {
            int checkbrand = override_brand ? override_brand
                                            : get_weapon_brand(item);
            // this is specialcased out of weapon_brand_name
            // ("vampiric hand axe", etc)
            if (checkbrand == SPWPN_VAMPIRISM)
                return "vampirism";
            else if (checkbrand == SPWPN_ANTIMAGIC)
                return "antimagic";
        }
        if (get_weapon_brand(item) != SPWPN_NORMAL)
            return weapon_brand_name(item, terse, override_brand);
        else
            return "";
    case OBJ_MISSILES:
        // HACKHACKHACK
        if (item.props.exists(DAMNATION_BOLT_KEY))
            return "damnation";
        return missile_brand_name(item, terse ? MBN_TERSE : MBN_BRAND);
    case OBJ_JEWELLERY:
        return jewellery_effect_name(item.sub_type, terse);
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
 * When naming the given item, should the curse be mentioned?
 */
static bool _know_curse(const item_def &item, description_level_type desc,
                        bool ident, iflags_t ignore_flags)
{
    return _know_ident(item, desc, ident, ignore_flags, ISFLAG_KNOW_CURSE);
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
 * Construct the name of a given deck item.
 *
 * @param[in] deck      The deck item in question.
 * @param[in] desc      The description level to be used.
 * @param[in] ident     Whether the deck should be named as if it were
 *                      identified.
 * @param[out] buff     The buffer to fill with the given item name.
 */
static void _name_deck(const item_def &deck, description_level_type desc,
                       bool ident, ostringstream &buff)
{
    const bool know_type = ident || item_type_known(deck);

    const bool dbname   = desc == DESC_DBNAME;
    const bool basename = _use_basename(deck, desc, ident);

    if (basename)
    {
        buff << "deck of cards";
        return;
    }

    if (bad_deck(deck))
    {
        buff << "BUGGY deck of cards";
        return;
    }

    if (!dbname)
        buff << deck_rarity_name(deck.deck_rarity) << ' ';

    if (deck.sub_type == MISC_DECK_UNKNOWN)
        buff << misc_type_name(MISC_DECK_OF_ESCAPE, false);
    else
        buff << misc_type_name(deck.sub_type, know_type);

    // name overriden, not a stacked deck, not a deck that's been drawn from
    if (dbname || !top_card_is_known(deck) && deck.used_count == 0)
        return;

    buff << " {";
    // A marked deck!
    if (top_card_is_known(deck))
        buff << card_name(top_card(deck));

    // How many cards have been drawn, or how many are left.
    if (deck.used_count != 0)
    {
        if (top_card_is_known(deck))
            buff << ", ";

        if (deck.used_count > 0)
            buff << "drawn: ";
        else
            buff << "left: ";

        buff << abs(deck.used_count);
    }

    buff << "}";
}

/**
 * The curse-describing prefix to a weapon's name, including trailing space if
 * appropriate. (Empty if the weapon isn't cursed, or if the curse shouldn't be
 * prefixed.)
 */
static string _curse_prefix(const item_def &weap, description_level_type desc,
                            bool terse, bool ident, iflags_t ignore_flags)
{
    if (!_know_curse(weap, desc, ident, ignore_flags) || terse)
        return "";

    if (weap.cursed())
        return "cursed ";

    // We don't bother printing "uncursed" if the item is identified
    // for pluses (its state should be obvious), this is so that
    // the weapon name is kept short (there isn't a lot of room
    // for the name on the main screen). If you're going to change
    // this behaviour, *please* make it so that there is an option
    // that maintains this behaviour. -- bwr
    if (_know_pluses(weap, desc, ident, ignore_flags))
        return "";
    // Nor for artefacts. Again, the state should be obvious. --jpeg
    if (!ident && !item_type_known(weap)
        || !is_artefact(weap))
    {
        return "uncursed ";
    }
    return "";
}

/**
 * The plus-describing prefix to a weapon's name, including trailing space.
 */
static string _plus_prefix(const item_def &weap)
{
    if (is_unrandom_artefact(weap, UNRAND_WOE))
        return "+∞ ";
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
 * The ego-describing prefix to a weapon's name, including trailing space if
 * appropriate. (Empty if the weapon's brand shouldn't be prefixed.)
 */
static string _ego_prefix(const item_def &weap, description_level_type desc,
                          bool terse, bool ident, iflags_t ignore_flags)
{
    if (!_know_ego(weap, desc, ident, ignore_flags) || terse)
        return "";

    switch (get_weapon_brand(weap))
    {
        case SPWPN_VAMPIRISM:
            return "vampiric ";
        case SPWPN_ANTIMAGIC:
            return "antimagic ";
        case SPWPN_NORMAL:
            if (!_know_pluses(weap, desc, ident, ignore_flags)
                && get_equip_desc(weap))
            {
                return "enchanted ";
            }
            // fallthrough to default
        default:
            return "";
    }
}

/**
 * The ego-describing suffix to a weapon's name, May be empty. Does not include
 * trailing space.
 */
static string _ego_suffix(const item_def &weap, bool terse)
{
    const string brand_name = weapon_brand_name(weap, terse);
    if (brand_name.empty())
        return "";

    if (terse)
        return make_stringf(" (%s)", brand_name.c_str());
    return " of " + brand_name;
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

    const bool know_curse =  _know_curse(weap, desc, ident, ignore_flags);
    const bool know_pluses = _know_pluses(weap, desc, ident, ignore_flags);
    const bool know_ego =    _know_ego(weap, desc, ident, ignore_flags);

    const string curse_prefix
        = _curse_prefix(weap, desc, terse, ident, ignore_flags);
    const string plus_text = know_pluses ? _plus_prefix(weap) : "";

    if (is_artefact(weap) && !dbname)
    {
        const string long_name = curse_prefix + plus_text
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
        if (is_unrandom_artefact(weap, UNRAND_JIHAD)
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
    const string ego_prefix
        = _ego_prefix(weap, desc, terse, ident, ignore_flags);
    const string ego_suffix = know_ego ? _ego_suffix(weap, terse) : "";
    const string curse_suffix
        = know_curse && weap.cursed() && terse ? " (curse)" :  "";
    return curse_prefix + plus_text + cosmetic_text + ego_prefix
           + item_base_name(weap)
           + ego_suffix + curse_suffix;
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

    const bool know_curse =  _know_curse(*this, desc, ident, ignore_flags);
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

        if (!terse && !dbname)
        {
            if (props.exists(DAMNATION_BOLT_KEY)) // hack alert
                buff << "damnation ";
            else if (_missile_brand_is_prefix(msl_brand))
                buff << missile_brand_name(*this, MBN_NAME) << ' ';
        }

        buff << ammo_name(static_cast<missile_type>(item_typ));

        if (msl_brand != SPMSL_NORMAL
#if TAG_MAJOR_VERSION == 34
            && msl_brand != SPMSL_BLINDING
#endif
            && !basename && !qualname && !dbname)
        {
            if (terse)
            {
                if (props.exists(DAMNATION_BOLT_KEY)) // still a hack
                    buff << " (damnation)";
                else
                    buff << " (" <<  missile_brand_name(*this, MBN_TERSE) << ")";
            }
            else if (_missile_brand_is_postfix(msl_brand))
                buff << " of " << missile_brand_name(*this, MBN_NAME);
        }

        break;
    }
    case OBJ_ARMOUR:
        if (know_curse && !terse)
        {
            if (cursed())
                buff << "cursed ";
            else if (!know_pluses)
                buff << "uncursed ";

        }

        // If we know enough to know it has *something* ('shiny' etc),
        // but we know it has no ego, it must have a plus. (or maybe a curse.)
        // If we don't know what the plus is, call it 'enchanted'.
        if (!terse && know_ego && get_armour_ego_type(*this) == SPARM_NORMAL &&
            !know_pluses && !is_artefact(*this) && get_equip_desc(*this))
        {
            buff << "enchanted ";
        }

        // Don't list QDA as +0.
        if (know_pluses && sub_type != ARM_QUICKSILVER_DRAGON_ARMOUR)
            buff << make_stringf("%+d ", plus);

        if (item_typ == ARM_GLOVES || item_typ == ARM_BOOTS)
            buff << "pair of ";

        if (is_artefact(*this) && !dbname)
        {
            buff << get_artefact_name(*this);
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

        if (know_curse && cursed() && terse)
            buff << " (curse)";
        break;

    case OBJ_WANDS:
        if (basename)
        {
            buff << "wand";
            break;
        }

        if (!dbname && props.exists(PAKELLAS_SUPERCHARGE_KEY))
            buff << "supercharged ";

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

        if (know_type || know_pluses) // probably know_type is sufficient?
        {
            buff << " (";
            if (know_pluses)
                buff << charges;
            else
                buff << "?";
            buff << "/" << wand_max_charges(*this) << ")";
        }

        if (!know_pluses && with_inscription)
        {
            if (used_count == ZAPCOUNT_EMPTY)
                buff << " {empty}";
            else if (used_count == ZAPCOUNT_RECHARGED)
                buff << " {recharged}";
            else if (used_count > 0)
                buff << " {zapped: " << used_count << '}';
        }
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

    case OBJ_FOOD:
        switch (item_typ)
        {
        case FOOD_MEAT_RATION: buff << "meat ration"; break;
        case FOOD_BREAD_RATION: buff << "bread ration"; break;
        case FOOD_ROYAL_JELLY: buff << "royal jelly"; break;
        case FOOD_FRUIT: buff << "fruit"; break;
        case FOOD_PIZZA: buff << "slice of pizza"; break;
        case FOOD_BEEF_JERKY: buff << "beef jerky"; break;
        case FOOD_CHUNK:
            switch (determine_chunk_effect(*this))
            {
                case CE_MUTAGEN:
                    buff << "mutagenic ";
                    break;
                case CE_NOXIOUS:
                    buff << "inedible ";
                    break;
                default:
                    break;
            }

            buff << "chunk of flesh";
            break;
#if TAG_MAJOR_VERSION == 34
        default: buff << "removed food"; break;
#endif
        }

        break;

    case OBJ_SCROLLS:
        buff << "scroll";
        if (basename)
            break;
        else
            buff << " ";

        if (know_type)
            buff << "of " << scroll_type_name(item_typ);
        else
            buff << "labeled " << make_name(subtype_rnd, MNAME_SCROLL);
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

        if (know_curse && !terse)
        {
            if (cursed())
                buff << "cursed ";
            else if (desc != DESC_PLAIN
                     && (!is_randart || !know_type)
                     && (!jewellery_has_pluses(*this) || !know_pluses)
                     // If the item is worn, its curse status is known,
                     // no need to belabour the obvious.
                     && get_equip_slot(this) == -1)
            {
                buff << "uncursed ";
            }
        }

        if (is_randart && !dbname)
        {
            buff << get_artefact_name(*this);
            break;
        }

        if (know_type)
        {
            if (know_pluses && jewellery_has_pluses(*this))
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
        if (know_curse && cursed() && terse)
            buff << " (curse)";
        break;
    }
    case OBJ_MISCELLANY:
        if (is_deck(*this) || item_typ == MISC_DECK_UNKNOWN)
        {
            _name_deck(*this, desc, ident, buff);
            break;
        }

        buff << misc_type_name(item_typ, know_type);

        if (is_xp_evoker(*this) && !dbname && !evoker_is_charged(*this))
            buff << " (inert)";

        break;

    case OBJ_BOOKS:
        if (is_random_artefact(*this) && !dbname && !basename)
        {
            buff << get_artefact_name(*this);
            if (!know_type)
                buff << "book";
            break;
        }
        if (basename)
            buff << (item_typ == BOOK_MANUAL ? "manual" : "book");
        else if (!know_type)
        {
            buff << book_secondary_string(rnd)
                 << book_primary_string(rnd) << " "
                 << (item_typ == BOOK_MANUAL ? "manual" : "book");
        }
        else
            buff << sub_type_string(*this, !dbname);
        break;

    case OBJ_RODS:
        if (!know_type)
        {
            if (!basename)
            {
                buff << staff_secondary_string((rnd / NDSC_STAVE_PRI) % NDSC_STAVE_SEC)
                     << staff_primary_string(rnd % NDSC_STAVE_PRI);
            }

            buff << "rod";
        }
        else
        {
            if (know_type && know_pluses && !basename && !qualname && !dbname)
                buff << make_stringf("%+d ", special);

            if (!dbname && props.exists(PAKELLAS_SUPERCHARGE_KEY))
                buff << "supercharged ";

            if (item_typ == ROD_LIGHTNING)
                buff << "lightning rod";
            else if (item_typ == ROD_IRON)
                buff << "iron rod";
            else
                buff << "rod of " << rod_type_name(item_typ);
        }
        break;

    case OBJ_STAVES:
        if (know_curse && !terse)
        {
            if (cursed())
                buff << "cursed ";
            else if (desc != DESC_PLAIN
                     && (!know_type || !is_artefact(*this)))
            {
                buff << "uncursed ";
            }
        }

        if (!know_type)
        {
            if (!basename)
            {
                buff << staff_secondary_string(subtype_rnd / NDSC_STAVE_PRI)
                     << staff_primary_string(subtype_rnd % NDSC_STAVE_PRI);
            }

            buff << "staff";
        }
        else
            buff << "staff of " << staff_type_name(item_typ);

        if (know_curse && cursed() && terse)
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

    default:
        buff << "!";
    }

    // One plural to rule them all.
    if (need_plural && quantity > 1 && !basename && !qualname)
        buff.str(pluralise(buff.str()));

    // Rod charges.
    if (base_type == OBJ_RODS && know_type && know_pluses
        && !basename && !qualname && !dbname)
    {
        buff << " (" << (charges / ROD_CHARGE_MULT)
             << "/"  << (charge_cap / ROD_CHARGE_MULT)
             << ")";
    }

    // debugging output -- oops, I probably block it above ... dang! {dlb}
    if (buff.str().length() < 3)
    {
        buff << "bad item (cl:" << static_cast<int>(base_type)
             << ",ty:" << item_typ << ",pl:" << plus
             << ",pl2:" << used_count << ",sp:" << special
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
    COMPILE_CHECK(NUM_FOODS      < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_SCROLLS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_JEWELLERY  < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_POTIONS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_BOOKS      < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_STAVES     < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_MISCELLANY < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_RODS       < MAX_SUBTYPES);

    return base_type == OBJ_WANDS || base_type == OBJ_SCROLLS
        || base_type == OBJ_JEWELLERY || base_type == OBJ_POTIONS
        || base_type == OBJ_STAVES || base_type == OBJ_BOOKS;
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

    // Artefacts have different descriptions from other items,
    // so we can't use general item knowledge for them.
    if (is_artefact(item))
        return false;

    if (item.base_type == OBJ_MISSILES)
        return true;

    if (item.base_type == OBJ_MISCELLANY && !is_deck(item))
        return true;

#if TAG_MAJOR_VERSION == 34
    if (item.is_type(OBJ_BOOKS, BOOK_BUGGY_DESTRUCTION))
        return true;
#endif

    if (item.is_type(OBJ_BOOKS, BOOK_MANUAL))
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

bool set_ident_type(item_def &item, bool identify)
{
    if (is_artefact(item) || crawl_state.game_is_arena())
        return false;

    if (!set_ident_type(item.base_type, item.sub_type, identify))
        return false;

    if (in_inventory(item))
    {
        shopping_list.cull_identical_items(item);
        if (identify)
            item_skills(item, you.start_train);
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

bool set_ident_type(object_class_type basetype, int subtype, bool identify)
{
    preserve_quiver_slots p;

    if (!item_type_has_ids(basetype))
        return false;

    if (you.type_ids[basetype][subtype] == identify)
        return false;

    you.type_ids[basetype][subtype] = identify;
    request_autoinscribe();

    // Our item knowledge changed in a way that could possibly affect shop
    // prices.
    shopping_list.item_type_identified(basetype, subtype);

    // We identified something, maybe we identified other things by process of
    // elimination.
    if (identify && !(you.pending_revival || crawl_state.updating_scores))
        _maybe_identify_pack_item();

    return true;
}

void pack_item_identify_message(int base_type, int sub_type)
{
    for (const auto &item : you.inv)
        if (item.defined() && item.is_type(base_type, sub_type))
            mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
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

class KnownMenu : public InvMenu
{
public:
    // This loads items in the order they are put into the list (sequentially)
    menu_letter load_items_seq(const vector<const item_def*> &mitems,
                               MenuEntry *(*procfn)(MenuEntry *me) = nullptr,
                               menu_letter ckey = 'a')
    {
        for (const item_def *item : mitems)
        {
            InvEntry *ie = new InvEntry(*item);
            if (tag == "pickup")
                ie->tag = "pickup";
            // If there's no hotkey, provide one.
            if (ie->hotkeys[0] == ' ')
                ie->hotkeys[0] = ckey++;
            do_preselect(ie);

            add_entry(procfn? (*procfn)(ie) : ie);
        }

        return ckey;
    }

protected:
    string help_key() const override
    {
        return "known-menu";
    }

    bool allow_easy_exit() const override
    {
        return true;
    }

    bool process_key(int key) override
    {
        bool resetting = (lastch == CONTROL('D'));
        if (resetting)
        {
            //return the menu title to its previous text.
            set_title(temp_title);
            update_title();
            num = -2;

            // Disarm ^D here, because process_key doesn't always set lastch.
            lastch = ' ';
        }
        else
            num = -1;

        switch (key)
        {
        case ',':
            return true;
        case '*':
            if (!resetting)
                break;
        case '^':
            key = ',';
            break;

        case '-':
        case '\\':
        case CK_ENTER:
        CASE_ESCAPE
            lastch = key;
            return false;

        case CONTROL('D'):
            // If we cannot select anything (e.g. on the unknown items
            // page), ignore Ctrl-D. Likewise if the last key was
            // Ctrl-D (we have already disarmed Ctrl-D for the next
            // keypress by resetting lastch).
            if (flags & (MF_SINGLESELECT | MF_MULTISELECT) && !resetting)
            {
                lastch = CONTROL('D');
                temp_title = title->text;
                set_title("Select to reset item to default: ");
                update_title();
            }

            return true;
        }
        return Menu::process_key(key);
    }
};

class KnownEntry : public InvEntry
{
public:
    KnownEntry(InvEntry* inv) : InvEntry(*inv->item)
    {
        hotkeys[0] = inv->hotkeys[0];
        selected_qty = inv->selected_qty;
    }

    virtual string get_text(bool need_cursor) const override
    {
        need_cursor = need_cursor && show_cursor;
        int flags = item->base_type == OBJ_WANDS ? 0 : ISFLAG_KNOW_PLUSES;

        string name;

        if (item->base_type == OBJ_FOOD)
        {
            switch (item->sub_type)
            {
            case FOOD_CHUNK:
                name = "chunks";
                break;
            case FOOD_MEAT_RATION:
                name = "meat rations";
                break;
            case FOOD_BEEF_JERKY:
                name = "beef jerky";
                break;
            case FOOD_BREAD_RATION:
                name = "bread rations";
                break;
#if TAG_MAJOR_VERSION == 34
            default:
#endif
            case FOOD_FRUIT:
                name = "fruit";
                break;
            case FOOD_PIZZA:
                name = "pizza";
                break;
            case FOOD_ROYAL_JELLY:
                name = "royal jellies";
                break;
            }
        }
        else if (item->base_type == OBJ_MISCELLANY)
        {
            if (item->sub_type == MISC_PHANTOM_MIRROR)
                name = pluralise(item->name(DESC_PLAIN));
            else
                name = "miscellaneous";
        }
        else if (item->is_type(OBJ_BOOKS, BOOK_MANUAL))
            name = "manuals";
        else if (item->base_type == OBJ_RODS || item->base_type == OBJ_GOLD)
        {
            name = lowercase_string(item_class_name(item->base_type));
            name = pluralise(name);
        }
        else if (item->base_type == OBJ_RUNES)
            name = "runes";
        else if (item->sub_type == get_max_subtype(item->base_type))
            name = "unknown " + lowercase_string(item_class_name(item->base_type));
        else
        {
            name = item->name(DESC_PLAIN,false,true,false,false,flags);
            name = pluralise(name);
        }

        char symbol;
        if (selected_qty == 0)
            symbol = item_needs_autopickup(*item) ? '+' : '-';
        else if (selected_qty == 1)
            symbol = '+';
        else
            symbol = '-';

        return make_stringf(" %c%c%c%c%s", hotkeys[0], need_cursor ? '[' : ' ',
                                           symbol, need_cursor ? ']' : ' ',
                                           name.c_str());
    }

    virtual int highlight_colour() const override
    {
        if (selected_qty >= 1)
            return WHITE;
        else
            return MENU_ITEM_STOCK_COLOUR;

    }

    virtual bool selected() const override
    {
        return selected_qty != 0 && quantity;
    }

    virtual void select(int qty) override
    {
        // Toggle  grey -> - -> + -> grey  if we autopickup the item by
        // default, or  grey -> + -> - -> grey  if we do not.
        if (qty == -2)
            selected_qty = 0;
        else if (selected_qty == 0)
            selected_qty = item_needs_autopickup(*item, true) ? 2 : 1;
        else if (selected_qty == (item_needs_autopickup(*item, true) ? 2 : 1))
            selected_qty = 3 - selected_qty; // 2 <-> 1
        else
            selected_qty = 0;

        // Set the force_autopickup values
        const int forceval = (selected_qty == 2 ? -1 : selected_qty);
        you.force_autopickup[item->base_type][item->sub_type] = forceval;
    }
};

class UnknownEntry : public InvEntry
{
public:
    UnknownEntry(InvEntry* inv) : InvEntry(*inv->item)
    {
    }

    virtual string get_text(const bool = false) const override
    {
        int flags = item->base_type == OBJ_WANDS ? 0 : ISFLAG_KNOW_PLUSES;

        return string(" ") + item->name(DESC_PLAIN, false, true, false,
                                        false, flags);
    }
};

static MenuEntry *known_item_mangle(MenuEntry *me)
{
    unique_ptr<InvEntry> ie(dynamic_cast<InvEntry*>(me));
    KnownEntry *newme = new KnownEntry(ie.get());
    return newme;
}

static MenuEntry *unknown_item_mangle(MenuEntry *me)
{
    unique_ptr<InvEntry> ie(dynamic_cast<InvEntry*>(me));
    UnknownEntry *newme = new UnknownEntry(ie.get());
    return newme;
}

static bool _identified_item_names(const item_def *it1,
                                   const item_def *it2)
{
    int flags = it1->base_type == OBJ_WANDS ? 0 : ISFLAG_KNOW_PLUSES;
    return it1->name(DESC_PLAIN, false, true, false, false, flags)
         < it2->name(DESC_PLAIN, false, true, false, false, flags);
}

// Allocate (with new) a new item_def with the given base and sub types,
// add a pointer to it to the items vector, and if it has a force_autopickup
// setting add a corresponding SelItem to selected.
static void _add_fake_item(object_class_type base, int sub,
                           vector<SelItem> &selected,
                           vector<const item_def*> &items,
                           bool force_known_type = false)
{
    item_def* ptmp = new item_def;

    ptmp->base_type = base;
    ptmp->sub_type  = sub;
    ptmp->quantity  = 1;
    ptmp->rnd       = 1;

    if (base == OBJ_WANDS && sub != NUM_WANDS)
        ptmp->charges = wand_max_charges(*ptmp);
    else if (base == OBJ_GOLD)
        ptmp->quantity = 18;
    else if (ptmp->is_type(OBJ_FOOD, FOOD_CHUNK))
    {
        ptmp->freshness = 100;
        ptmp->mon_type = MONS_RAT;
    }
    else if (is_deck(*ptmp, true)) // stupid fake decks
        ptmp->deck_rarity = DECK_RARITY_COMMON;

    if (force_known_type)
        ptmp->flags |= ISFLAG_KNOW_TYPE;

    items.push_back(ptmp);

    if (you.force_autopickup[base][sub] == 1)
        selected.emplace_back(0, 1, ptmp);
    else if (you.force_autopickup[base][sub] == -1)
        selected.emplace_back(0, 2, ptmp);
}

void check_item_knowledge(bool unknown_items)
{
    vector<const item_def*> items;
    vector<const item_def*> items_missile; //List of missiles should go after normal items
    vector<const item_def*> items_food;    //List of foods should come next
    vector<const item_def*> items_other;   //List of other items should go after everything
    vector<SelItem> selected_items;

    bool all_items_known = true;
    for (int ii = 0; ii < NUM_OBJECT_CLASSES; ii++)
    {
        object_class_type i = (object_class_type)ii;
        if (!item_type_has_ids(i))
            continue;
        for (int j = 0; j < get_max_subtype(i); j++)
        {
            if (i == OBJ_JEWELLERY && j >= NUM_RINGS && j < AMU_FIRST_AMULET)
                continue;

            if (i == OBJ_BOOKS && j > MAX_FIXED_BOOK)
                continue;

            if (item_type_removed(i, j))
                continue;

            if (you.type_ids[i][j] != unknown_items) // logical xor
                _add_fake_item(i, j, selected_items, items, !unknown_items);
            else
                all_items_known = false;
        }
    }

    if (unknown_items)
        all_items_known = false;
    else
    {
        // items yet to be known
        for (int ii = 0; ii < NUM_OBJECT_CLASSES; ii++)
        {
            object_class_type i = (object_class_type)ii;
            if (!item_type_has_ids(i))
                continue;
            _add_fake_item(i, get_max_subtype(i), selected_items, items);
        }
        // Missiles
        for (int i = 0; i < NUM_MISSILES; i++)
        {
#if TAG_MAJOR_VERSION == 34
            if (i == MI_DART)
                continue;
#endif
            _add_fake_item(OBJ_MISSILES, i, selected_items, items_missile);
        }
        // Foods
        for (int i = 0; i < NUM_FOODS; i++)
        {
#if TAG_MAJOR_VERSION == 34
            if (!is_real_food(static_cast<food_type>(i)))
                continue;
#endif
            _add_fake_item(OBJ_FOOD, i, selected_items, items_food);
        }

        // Misc.
        static const pair<object_class_type, int> misc_list[] =
        {
            { OBJ_BOOKS, BOOK_MANUAL },
            { OBJ_RODS, NUM_RODS },
            { OBJ_GOLD, 1 },
            { OBJ_MISCELLANY, NUM_MISCELLANY },
            { OBJ_RUNES, NUM_RUNE_TYPES },
        };
        for (auto e : misc_list)
            _add_fake_item(e.first, e.second, selected_items, items_other);
    }

    sort(items.begin(), items.end(), _identified_item_names);
    sort(items_missile.begin(), items_missile.end(), _identified_item_names);
    sort(items_food.begin(), items_food.end(), _identified_item_names);

    KnownMenu menu;
    string stitle;

    if (unknown_items)
        stitle = "Items not yet recognised: (toggle with -)";
    else if (!all_items_known)
        stitle = "Recognised items. (- for unrecognised, select to toggle autopickup)";
    else
        stitle = "You recognise all items. (Select to toggle autopickup)";

    string prompt = "(_ for help)";
    //TODO: when the menu is opened, the text is not justified properly.
    stitle = stitle + string(max(0, get_number_of_cols() - strwidth(stitle)
                                                         - strwidth(prompt)),
                             ' ') + prompt;

    menu.set_preselect(&selected_items);
    menu.set_flags( MF_QUIET_SELECT | MF_ALLOW_FORMATTING
                    | ((unknown_items) ? MF_NOSELECT
                                       : MF_MULTISELECT | MF_ALLOW_FILTER));
    menu.set_type(MT_KNOW);
    menu_letter ml;
    ml = menu.load_items(items, unknown_items ? unknown_item_mangle
                                              : known_item_mangle, 'a', false);

    ml = menu.load_items(items_missile, known_item_mangle, ml, false);
    ml = menu.load_items(items_food, known_item_mangle, ml, false);
    if (!items_other.empty())
    {
        menu.add_entry(new MenuEntry("Other Items", MEL_SUBTITLE));
        ml = menu.load_items_seq(items_other, known_item_mangle, ml);
    }

    menu.set_title(stitle);
    menu.show(true);

    auto last_char = menu.getkey();

    deleteAll(items);
    deleteAll(items_missile);
    deleteAll(items_food);
    deleteAll(items_other);

    if (!all_items_known && (last_char == '\\' || last_char == '-'))
        check_item_knowledge(!unknown_items);
}

static MenuEntry* _fixup_runeorb_entry(MenuEntry* me)
{
    auto entry = static_cast<InvEntry*>(me);
    ASSERT(entry);

    if (entry->item->base_type == OBJ_RUNES)
    {
        auto rune = static_cast<rune_type>(entry->item->sub_type);
        colour_t colour;
        // Make Gloorx's rune more distinguishable from uncollected runes.
        if (you.runes[rune])
            colour = (rune == RUNE_GLOORX_VLOQ) ? LIGHTGREY : rune_colour(rune);
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
        // Use the generic tile for rune that haven't been gotten yet, to make
        // it more clear at a glance.
        if (!you.runes[rune])
            const_cast<item_def*>(entry->item)->sub_type = NUM_RUNE_TYPES;
    }
    else if (entry->item->is_type(OBJ_ORBS, ORB_ZOT))
    {
        if (player_has_orb())
            entry->text = "<magenta>The Orb of Zot</magenta>";
        else
        {
            entry->text = "<darkgrey>The Orb of Zot"
                          " (the Realm of Zot)</darkgrey>";
        }
    }

    return entry;
}

void display_runes()
{
    auto col = runes_in_pack() < ZOT_ENTRY_RUNES ?  "lightgrey" :
               runes_in_pack() < you.obtainable_runes ? "green" :
                                                   "lightgreen";

    auto title = make_stringf("<white>Runes of Zot (</white>"
                              "<%s>%d</%s><white> collected) & Orbs of Power</white>",
                              col, runes_in_pack(), col);
    title = string(max(0, 39 - printed_width(title) / 2), ' ') + title;

    InvMenu menu(MF_NOSELECT | MF_ALLOW_FORMATTING);

    menu.set_title(title);

    vector<item_def> items;

    if (!crawl_state.game_is_sprint())
    {
        // Add the runes in order of challenge (semi-arbitrary).
        for (branch_iterator it(BRANCH_ITER_DANGER); it; ++it)
        {
            const branch_type br = it->id;
            if (!connected_branch_can_exist(br))
                continue;

            for (auto rune : branches[br].runes)
            {
                item_def item;
                item.base_type = OBJ_RUNES;
                item.sub_type = rune;
                item.quantity = 1;
                item_colour(item);
                items.push_back(item);
            }
        }
    }
    else
    {
        // We don't know what runes are accessible in the sprint, so just show
        // the ones you have. We can't iterate over branches as above since the
        // elven rune and mossy rune may exist in sprint.
        for (int i = 0; i < NUM_RUNE_TYPES; ++i)
        {
            if (you.runes[i])
            {
                item_def item;
                item.base_type = OBJ_RUNES;
                item.sub_type = i;
                item.quantity = 1;
                item_colour(item);
                items.push_back(item);
            }
        }
    }
    item_def item;
    item.base_type = OBJ_ORBS;
    item.sub_type = ORB_ZOT;
    item.quantity = 1;
    items.push_back(item);

    // We've sorted this vector already, so disable menu sorting. Maybe we
    // could a menu entry comparator and modify InvMenu::load_items() to allow
    // passing this in instead of doing a sort ahead of time.
    menu.load_items(items, _fixup_runeorb_entry, 0, false);

    menu.show();
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
 *                  By default a random number from the RNG.
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
    uint64_t sarg[1] = { static_cast<uint64_t>(seed) };
    PcgRNG rng = PcgRNG(sarg, ARRAYSZ(sarg));

    string name;

    bool has_space  = false; // Keep track of whether the name contains a space.

    size_t len = 3 + rng.get_uint32() % 5
                   + ((rng.get_uint32() % 5 == 0) ? rng.get_uint32() % 6 : 1);

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
            name += 'a' + (rng.get_uint32() % 26);
        }
        else if (!has_space && name_type != MNAME_JIYVA
                 && name.length() > 5 && name.length() < len - 4
                 && rng.get_uint32() % 5 != 0) // 4/5 chance
        {
             // Hand out a space.
            name += ' ';
        }
        else if (name.length()
                 && (_is_consonant(prev_char)
                     || (name.length() > 1
                         && !_is_consonant(prev_char)
                         && _is_consonant(penult_char)
                         && rng.get_uint32() % 5 <= 1))) // 2/5
        {
            // Place a vowel.
            const char vowel = _random_vowel(rng.get_uint32());

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
                         || rng.get_uint32() % 5 <= 1))
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
                && rng.get_uint32() % 7 <= 1 // 2/7 chance
                && (!beg || !end))
            {
                const int first = (beg ? RCS_BB : (end ? RCS_BE : RCS_BM));
                const int last  = (beg ? RCS_EB : (end ? RCS_EE : RCS_EM));

                const int range = last - first;

                const int cons_seed = rng.get_uint32() % range + first;

                const string consonant_set = _random_consonant_set(cons_seed);

                ASSERT(consonant_set.size() > 1);
                len += consonant_set.size() - 2; // triples increase len
                name += consonant_set;
            }
            else // Place a single letter instead.
            {
                // Pick a random consonant.
                name += _random_cons(rng.get_uint32());
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
                && rng.get_uint32() % 3 != 0))) // 2/3 chance for other short names
    {
        // Specifically, add a consonant.
        name += _random_cons(rng.get_uint32());
    }

    if (maxlen != SIZE_MAX)
        name = chop_string(name, maxlen);
    trim_string_right(name);

    // Fallback if the name was too short.
    if (name.length() < 4)
    {
        // convolute & recurse
        if (name_type == MNAME_JIYVA)
            return make_name(rng.get_uint32(), MNAME_JIYVA);

        name = "plog";
    }

    string uppercased_name;
    for (size_t i = 0; i < name.length(); i++)
    {
        if (name_type == MNAME_JIYVA)
            ASSERT(name[i] != ' ');

        if (name_type == MNAME_SCROLL || i == 0 || name[i - 1] == ' ')
            uppercased_name += toupper(name[i]);
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
static char _random_vowel(int seed)
{
    static const char vowels[] = "aeiouaeiouaeiouy  ";
    return vowels[ seed % (sizeof(vowels) - 1) ];
}

// Returns a random consonant with not quite equal probability.
// Does not include 'y'.
static char _random_cons(int seed)
{
    static const char consonants[] = "bcdfghjklmnpqrstvwxzcdfghlmnrstlmnrst";
    return consonants[ seed % (sizeof(consonants) - 1) ];
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
static string _random_consonant_set(int seed)
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

    ASSERT_RANGE(seed, 0, (int) ARRAYSZ(consonant_sets));

    return consonant_sets[seed];
}

/**
 * Write all possible scroll names to the given file.
 */
static void _test_scroll_names(const string& fname)
{
    FILE *f = fopen(fname.c_str(), "w");
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
    FILE *f = fopen(fname.c_str(), "w");
    if (!f)
        sysfail("can't write test output");

    string longest;
    seed_rng(27);
    for (int i = 0; i < 1000000; i++)
    {
        const string name = make_name(get_uint32(), MNAME_JIYVA);
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

    seed_rng(27);
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
            return you.species != SP_FORMICID;
        case SCR_FEAR:
        case SCR_FOG:
            return true;
        default:
            return false;
        }
    case OBJ_POTIONS:
        if (you.species == SP_MUMMY)
            return false;

        switch (item.sub_type)
        {
        case POT_HASTE:
            return !have_passive(passive_t::no_haste)
                && you.species != SP_FORMICID;
        case POT_HEAL_WOUNDS:
            return you.can_potion_heal();
        case POT_CURING:
        case POT_RESISTANCE:
        case POT_MAGIC:
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
        return item.sub_type == SCR_ACQUIREMENT;
    case OBJ_POTIONS:
        if (you.species == SP_MUMMY)
            return false;
        switch (item.sub_type)
        {
        case POT_CURE_MUTATION:
#if TAG_MAJOR_VERSION == 34
        case POT_GAIN_STRENGTH:
        case POT_GAIN_INTELLIGENCE:
        case POT_GAIN_DEXTERITY:
#endif
        case POT_EXPERIENCE:
            return true;
        case POT_BENEFICIAL_MUTATION:
            return you.species != SP_GHOUL; // Mummies are already handled
        default:
            return false;
        }
    default:
        return false;
    }
}

/**
 * Is an item strictly harmful?
 *
 * @param item The item being queried.
 * @param temp Should temporary conditions such as transformations and
 *             vampire hunger levels be taken into account?  Religion (but
 *             not its absence) is considered to be permanent here.
 * @return True if the item is known to have only harmful effects.
 */
bool is_bad_item(const item_def &item, bool temp)
{
    if (!item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case SCR_CURSE_ARMOUR:
        case SCR_CURSE_WEAPON:
            if (you.species == SP_FELID)
                return false;
        case SCR_CURSE_JEWELLERY:
            return !have_passive(passive_t::want_curses);
#endif
        case SCR_NOISE:
            return true;
        default:
            return false;
        }
    case OBJ_POTIONS:
        // Can't be bad if you can't use them.
        if (you.species == SP_MUMMY)
            return false;

        switch (item.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case POT_SLOWING:
            return !you.stasis();
#endif
        case POT_DEGENERATION:
            return true;
#if TAG_MAJOR_VERSION == 34
        case POT_DECAY:
            return you.res_rotting(temp) <= 0;
        case POT_STRONG_POISON:
        case POT_POISON:
            // Poison is not that bad if you're poison resistant.
            return player_res_poison(false) <= 0
                   || !temp && you.species == SP_VAMPIRE;
#endif
        default:
            return false;
        }
    case OBJ_JEWELLERY:
        // Potentially useful. TODO: check the properties.
        if (is_artefact(item))
            return false;

        switch (item.sub_type)
        {
        case AMU_INACCURACY:
        case RING_LOUDNESS:
            return true;
        case RING_TELEPORTATION:
            return !(you.stasis() || crawl_state.game_is_sprint());
        case RING_EVASION:
        case RING_PROTECTION:
        case RING_STRENGTH:
        case RING_DEXTERITY:
        case RING_INTELLIGENCE:
        case RING_SLAYING:
            return item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus <= 0;
        default:
            return false;
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
 *             vampire hunger levels be taken into account?  Religion (but
 *             not its absence) is considered to be permanent here.
 * @return True if using the item is known to be risky but occasionally
 *         worthwhile.
 */
bool is_dangerous_item(const item_def &item, bool temp)
{
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
            return true;
        case SCR_TORMENT:
            return !player_mutation_level(MUT_TORMENT_RESISTANCE)
                   || !temp && you.species == SP_VAMPIRE;
        case SCR_HOLY_WORD:
            return you.undead_or_demonic();
        default:
            return false;
        }

    case OBJ_POTIONS:
        switch (item.sub_type)
        {
        case POT_MUTATION:
        case POT_LIGNIFY:
            return true;
        default:
            return false;
        }

    default:
        return false;
    }
}

static bool _invisibility_is_useless(const bool temp)
{
    // If you're Corona'd or a TSO-ite, this is always useless.
    return temp ? you.backlit()
                : you.haloed() && will_have_passive(passive_t::halo);
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
 *             vampire hunger levels be taken into account? Religion (but
 *             not its absence) is considered to be permanent here.
 * @return True if the item is known to be useless.
 */
bool is_useless_item(const item_def &item, bool temp)
{
    // During game startup, no item is useless. If someone re-glyphs an item
    // based on its uselessness, the glyph-to-item cache will use the useless
    // value even if your god or species can make use of it.
    if (you.species == SP_UNKNOWN)
        return false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (you.species == SP_FELID)
            return true;

        if (!you.could_wield(item, true, !temp)
            && !is_throwable(&you, item))
        {
            // Weapon is too large (or small) to be wielded and cannot
            // be thrown either.
            return true;
        }

        if (!item_type_known(item))
            return false;

        if (you.undead_or_demonic() && is_holy_item(item))
        {
            if (!temp && you.form == TRAN_LICH
                && you.species != SP_DEMONSPAWN)
            {
                return false;
            }
            return true;
        }

        return false;

    case OBJ_MISSILES:
        if ((you.has_spell(SPELL_STICKS_TO_SNAKES)
                || !you.num_turns && you.char_class == JOB_TRANSMUTER)
                && item.sub_type == MI_ARROW
            || (you.has_spell(SPELL_SANDBLAST)
                || !you.num_turns && you.char_class == JOB_EARTH_ELEMENTALIST)
                && item.sub_type == MI_STONE)
        {
            return false;
        }

        // Save for the above spells, all missiles are useless for felids.
        if (you.species == SP_FELID)
            return true;

        // These are the same checks as in is_throwable(), except that
        // we don't take launchers into account.
        switch (item.sub_type)
        {
        case MI_LARGE_ROCK:
            return !you.can_throw_large_rocks();
        case MI_JAVELIN:
            return you.body_size(PSIZE_BODY, !temp) < SIZE_MEDIUM
                   && !you.can_throw_large_rocks();
        }

        return false;

    case OBJ_ARMOUR:
        return !can_wear_armour(item, false, true)
                || (is_shield(item) && player_mutation_level(MUT_MISSING_HAND));

    case OBJ_SCROLLS:
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_LAVA_ORC && temperature_effect(LORC_NO_SCROLLS))
            return true;
#endif

        if (temp && silenced(you.pos()))
            return true; // can't use scrolls while silenced

        if (!item_type_known(item))
            return false;

        // A bad item is always useless.
        if (is_bad_item(item, temp))
            return true;

        switch (item.sub_type)
        {
        case SCR_RANDOM_USELESSNESS:
            return true;
        case SCR_TELEPORTATION:
            return you.species == SP_FORMICID
                   || crawl_state.game_is_sprint();
        case SCR_BLINKING:
            return you.species == SP_FORMICID;
        case SCR_AMNESIA:
            return you_worship(GOD_TROG);
#if TAG_MAJOR_VERSION == 34
        case SCR_CURSE_WEAPON: // for non-Ashenzari, already handled
        case SCR_CURSE_ARMOUR:
#endif
        case SCR_ENCHANT_WEAPON:
        case SCR_ENCHANT_ARMOUR:
        case SCR_BRAND_WEAPON:
            return you.species == SP_FELID;
        case SCR_SUMMONING:
            return player_mutation_level(MUT_NO_LOVE) > 0;
        case SCR_RECHARGING:
            return player_mutation_level(MUT_NO_ARTIFICE) > 0;
        case SCR_FOG:
            return env.level_state & LSTATE_STILL_WINDS;
        default:
            return false;
        }

    case OBJ_WANDS:
        if (player_mutation_level(MUT_NO_ARTIFICE))
            return true;

        if (item.sub_type == WAND_ENSLAVEMENT
            && item_type_known(item)
            && player_mutation_level(MUT_NO_LOVE))
        {
            return true;
        }

        if (you.magic_points < wand_mp_cost() && temp)
            return true;

        return is_known_empty_wand(item);

    case OBJ_POTIONS:
    {
        // Mummies can't use potions.
        if (you.undead_state(temp) == US_UNDEAD)
            return true;

        if (!item_type_known(item))
            return false;

        // A bad item is always useless.
        if (is_bad_item(item, temp))
            return true;

        switch (item.sub_type)
        {
        case POT_BERSERK_RAGE:
            return you.undead_state(temp)
                   && (you.species != SP_VAMPIRE
                       || temp && you.hunger_state < HS_SATIATED)
                   || you.species == SP_FORMICID;
        case POT_HASTE:
            return you.species == SP_FORMICID;

        case POT_CURE_MUTATION:
        case POT_MUTATION:
        case POT_BENEFICIAL_MUTATION:
#if TAG_MAJOR_VERSION == 34
        case POT_GAIN_STRENGTH:
        case POT_GAIN_INTELLIGENCE:
        case POT_GAIN_DEXTERITY:
#endif
            return !you.can_safely_mutate(temp);

        case POT_LIGNIFY:
            return you.undead_state(temp)
                   && (you.species != SP_VAMPIRE
                       || temp && you.hunger_state < HS_SATIATED);

        case POT_FLIGHT:
            return you.permanent_flight();

#if TAG_MAJOR_VERSION == 34
        case POT_PORRIDGE:
            return you.species == SP_VAMPIRE
                    || player_mutation_level(MUT_CARNIVOROUS) == 3;
        case POT_BLOOD_COAGULATED:
#endif
        case POT_BLOOD:
            return you.species != SP_VAMPIRE;
#if TAG_MAJOR_VERSION == 34
        case POT_DECAY:
            return you.res_rotting(temp) > 0;
        case POT_STRONG_POISON:
        case POT_POISON:
            // If you're poison resistant, poison is only useless.
            return !is_bad_item(item, temp);
        case POT_SLOWING:
            return you.species == SP_FORMICID;
#endif
        case POT_HEAL_WOUNDS:
            return !you.can_potion_heal();
        case POT_INVISIBILITY:
            return _invisibility_is_useless(temp);
        }

        return false;
    }
    case OBJ_JEWELLERY:
        if (!item_type_known(item))
            return false;

        // Potentially useful. TODO: check the properties.
        if (is_artefact(item))
            return false;

        if (is_bad_item(item, temp))
            return true;

        switch (item.sub_type)
        {
        case AMU_RAGE:
            return you.undead_state(temp)
                   && (you.species != SP_VAMPIRE
                       || temp && you.hunger_state < HS_SATIATED)
                   || you.species == SP_FORMICID
                   || player_mutation_level(MUT_NO_ARTIFICE);

        case RING_RESIST_CORROSION:
            return you.res_corr(false, false);

        case AMU_THE_GOURMAND:
            return player_likes_chunks(true) == 3
                   || player_mutation_level(MUT_GOURMAND) > 0
                   || player_mutation_level(MUT_HERBIVOROUS) == 3
                   || you.undead_state(temp);

        case AMU_FAITH:
            return (you.species == SP_DEMIGOD && !you.religion)
                    || you_worship(GOD_GOZAG)
                    || (you_worship(GOD_RU) && you.piety == piety_breakpoint(5));

        case AMU_GUARDIAN_SPIRIT:
            return you.spirit_shield(false, false);

        case RING_LIFE_PROTECTION:
            return player_prot_life(false, temp, false) == 3;

        case AMU_REGENERATION:
            return (player_mutation_level(MUT_SLOW_REGENERATION) == 3)
                   || temp && you.species == SP_VAMPIRE
                      && you.hunger_state <= HS_STARVING;

        case AMU_MANA_REGENERATION:
            return you_worship(GOD_PAKELLAS);

        case RING_SEE_INVISIBLE:
            return you.innate_sinv();

        case RING_POISON_RESISTANCE:
            return player_res_poison(false, temp, false) > 0
                   && (temp || you.species != SP_VAMPIRE);

        case RING_WIZARDRY:
            return you_worship(GOD_TROG);

        case RING_TELEPORTATION:
            return !is_bad_item(item, temp);

        case RING_FLIGHT:
            return you.permanent_flight()
                   || player_mutation_level(MUT_NO_ARTIFICE);

        case RING_STEALTH:
            return player_mutation_level(MUT_NO_STEALTH);

        default:
            return false;
        }

    case OBJ_RODS:
        if (you.species == SP_FELID
            || player_mutation_level(MUT_NO_ARTIFICE))
        {
            return true;
        }
        switch (item.sub_type)
        {
            case ROD_CLOUDS:
                if (item_type_known(item))
                    return env.level_state & LSTATE_STILL_WINDS;
                break;
            case ROD_SHADOWS:
                if (item_type_known(item))
                    return player_mutation_level(MUT_NO_LOVE);
                // intentional fallthrough
            default:
                return false;
        }
        break;

    case OBJ_STAVES:
        if (you.species == SP_FELID)
            return true;
        if (!you.could_wield(item, true, !temp))
        {
            // Weapon is too large (or small) to be wielded and cannot
            // be thrown either.
            return true;
        }
        if (!item_type_known(item))
            return false;

        switch (item.sub_type)
        {
        case STAFF_WIZARDRY:
        case STAFF_CONJURATION:
        case STAFF_SUMMONING:
            return you_worship(GOD_TROG);
        }

        return false;

    case OBJ_FOOD:
        if (item.sub_type == NUM_FOODS)
            break;

        if (!is_inedible(item))
            return false;

        if (!temp && you.form == TRAN_LICH)
        {
            // See what would happen if we were in our normal state.
            unwind_var<transformation_type> formsim(you.form, TRAN_NONE);

            if (!is_inedible(item))
                return false;
        }

        if (is_fruit(item) && you_worship(GOD_FEDHAS))
            return false;

        return true;

    case OBJ_CORPSES:
        if (item.sub_type != CORPSE_SKELETON && !you_foodless())
            return false;

        if (you.has_spell(SPELL_ANIMATE_DEAD)
            || you.has_spell(SPELL_ANIMATE_SKELETON)
            || you.has_spell(SPELL_SIMULACRUM)
            || you_worship(GOD_YREDELEMNUL) && !you.penance[GOD_YREDELEMNUL]
               && you.piety >= piety_breakpoint(0))
        {
            return false;
        }

        return true;

    case OBJ_MISCELLANY:
        switch (item.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case MISC_BUGGY_EBONY_CASKET:
            return item_type_known(item);
#endif
        // These can always be used.
#if TAG_MAJOR_VERSION == 34
        case MISC_BUGGY_LANTERN_OF_SHADOWS:
#endif
        case MISC_ZIGGURAT:
            return false;

        // Purely summoning misc items don't work w/ sac love
        case MISC_SACK_OF_SPIDERS:
        case MISC_BOX_OF_BEASTS:
        case MISC_HORN_OF_GERYON:
        case MISC_PHANTOM_MIRROR:
            return player_mutation_level(MUT_NO_LOVE)
                   || player_mutation_level(MUT_NO_ARTIFICE);

        default:
            return player_mutation_level(MUT_NO_ARTIFICE) && !is_deck(item);
        }

    case OBJ_BOOKS:
        if (!item_type_known(item) || item.sub_type != BOOK_MANUAL)
            return false;
        if (you.skills[item.plus] >= 27)
            return true;
        if (is_useless_skill((skill_type)item.plus))
            return true;
        return false;

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

    // Sometimes this is abbreviated out of the item name.
    if (item_type_has_curses(item.base_type)
        && item_ident(item, ISFLAG_KNOW_CURSE) && !item.cursed())
    {
        prefixes.push_back("uncursed");
    }

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
    if (is_bad_item(item, temp))
        prefixes.push_back("bad_item");
    if (is_useless_item(item, temp))
        prefixes.push_back("useless_item");

    if (item_is_stationary(item))
        prefixes.push_back("stationary");

    switch (item.base_type)
    {
    case OBJ_CORPSES:
        // Skeletons cannot be eaten.
        if (item.sub_type == CORPSE_SKELETON)
        {
            prefixes.push_back("inedible");
            break;
        }
        // intentional fall-through
    case OBJ_FOOD:
        // this seems like a big horrible gotcha waiting to happen
        if (item.sub_type == NUM_FOODS)
            break;

        if (is_inedible(item))
            prefixes.push_back("inedible");
        else if (is_preferred_food(item))
            prefixes.push_back("preferred");

        if (is_forbidden_food(item))
            prefixes.push_back("forbidden");

        if (is_mutagenic(item))
            prefixes.push_back("mutagenic");
        else if (is_noxious(item))
            prefixes.push_back("inedible");
        break;

    case OBJ_POTIONS:
        if (is_good_god(you.religion) && item_type_known(item)
            && is_blood_potion(item))
        {
            prefixes.push_back("evil_eating");
            prefixes.push_back("forbidden");
        }
        if (is_preferred_food(item))
        {
            prefixes.push_back("preferred");
            prefixes.push_back("food");
        }
        break;

    case OBJ_STAVES:
    case OBJ_WEAPONS:
        if (is_range_weapon(item))
            prefixes.push_back("ranged");
        else if (is_melee_weapon(item)) // currently redundant
            prefixes.push_back("melee");
        // fall through

    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
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
    const string cprf      = item_prefix(item);
    const string item_name = item.name(desc);

    const int col = menu_colour(item_name, cprf, "pickup");
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
        object_class_type base_type = static_cast<object_class_type>(i);
        const int num_sub_types = get_max_subtype(base_type);

        for (int sub_type = 0; sub_type < num_sub_types; sub_type++)
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
            if (base_type == OBJ_BOOKS && sub_type == BOOK_MANUAL)
                npluses = NUM_SKILLS;

            item_def item;
            item.base_type = base_type;
            item.sub_type = sub_type;
            for (int plus = 0; plus <= npluses; plus++)
            {
                if (plus > 0)
                    item.plus = max(0, plus - 1);
                if (is_deck(item))
                {
                    item.plus = 1;
                    item.deck_rarity = DECK_RARITY_COMMON;
                    init_deck(item);
                }
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
                    crawl_state.add_startup_error("Bad name for item name"
                                                  " cache: " + name);
                    continue;
                }

                if (!item_names_cache.count(name))
                {
                    item_names_cache[name] = { base_type, (uint8_t)sub_type,
                                               (int8_t)item.plus, 0 };
                    if (g.ch)
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

/*
 *  File:       itemname.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "itemname.h"

#include <sstream>
#include <iomanip>
#include <ctype.h>
#include <string.h>

#ifdef DOS
#include <conio.h>
#endif
#include "clua.h"

#include "externs.h"

#include "decks.h"
#include "food.h"
#include "initfile.h"
#include "invent.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "makeitem.h"
#include "mon-util.h"
#include "notes.h"
#include "player.h"
#include "quiver.h"
#include "randart.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "state.h"
#include "stuff.h"
#include "view.h"
#include "items.h"


#include "describe.h"


id_arr type_ids;

static bool is_random_name_space( char let );
static bool is_random_name_vowel( char let );

static char retvow(int sed);
static char retlet(int sed);

bool is_vowel( const char chr )
{
    const char low = tolower( chr );
    return (low == 'a' || low == 'e' || low == 'i' ||
            low == 'o' || low == 'u');
}

// quant_name is useful since it prints out a different number of items
// than the item actually contains.
std::string quant_name( const item_def &item, int quant,
                        description_level_type des, bool terse )
{
    // item_name now requires a "real" item, so we'll mangle a tmp
    item_def tmp = item;
    tmp.quantity = quant;

    return tmp.name(des, terse);
}

std::string item_def::name(description_level_type descrip,
                           bool terse, bool ident,
                           bool with_inscription,
                           bool quantity_in_words,
                           unsigned long ignore_flags) const
{
    if (crawl_state.arena)
    {
        ignore_flags |= ISFLAG_KNOW_PLUSES | ISFLAG_KNOW_CURSE
                        | ISFLAG_COSMETIC_MASK;
    }

    if (descrip == DESC_NONE)
        return ("");

    std::ostringstream buff;

    const std::string auxname = this->name_aux(descrip, terse, ident,
                                               ignore_flags);

    const bool startvowel     = is_vowel(auxname[0]);

    if (descrip == DESC_INVENTORY_EQUIP || descrip == DESC_INVENTORY)
    {
        if (in_inventory(*this)) // actually in inventory
        {
            buff << index_to_letter(this->link);
            if (terse)
                buff << ") ";
            else
                buff << " - ";
        }
        else
            descrip = DESC_CAP_A;
    }

    if (base_type == OBJ_BOOKS && (ident || item_type_known(*this))
        && book_has_title(*this))
    {
        if (descrip != DESC_DBNAME)
            descrip = DESC_PLAIN;
    }

    if (terse && descrip != DESC_DBNAME)
        descrip = DESC_PLAIN;

    if (base_type == OBJ_CORPSES && is_named_corpse(*this)
        && !starts_with(get_corpse_name(*this), "shaped "))
    {
        switch (descrip)
        {
        case DESC_CAP_A:
        case DESC_CAP_YOUR:
            descrip = DESC_CAP_THE;
            break;

        case DESC_NOCAP_A:
        case DESC_NOCAP_YOUR:
        case DESC_NOCAP_ITS:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
            descrip = DESC_NOCAP_THE;
            break;

        default:
            break;
        }
    }

    if (this->base_type == OBJ_ORBS
        || (ident || item_type_known( *this ))
            && (this->base_type == OBJ_MISCELLANY
                   && this->sub_type == MISC_HORN_OF_GERYON
                || is_artefact(*this)))
    {
        // Artefacts always get "the" unless we just want the plain name.
        switch (descrip)
        {
        case DESC_CAP_A:
        case DESC_CAP_YOUR:
        case DESC_CAP_THE:
            buff << "The ";
            break;
        case DESC_NOCAP_A:
        case DESC_NOCAP_YOUR:
        case DESC_NOCAP_THE:
        case DESC_NOCAP_ITS:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
            buff << "the ";
            break;
        default:
        case DESC_PLAIN:
            break;
        }
    }
    else if (this->quantity > 1)
    {
        switch (descrip)
        {
        case DESC_CAP_THE:    buff << "The "; break;
        case DESC_NOCAP_THE:  buff << "the "; break;
        case DESC_CAP_YOUR:   buff << "Your "; break;
        case DESC_NOCAP_YOUR: buff << "your "; break;
        case DESC_NOCAP_ITS:  buff << "its "; break;
        case DESC_CAP_A:
        case DESC_NOCAP_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
        case DESC_PLAIN:
        default:
            break;
        }

        if (descrip != DESC_BASENAME && descrip != DESC_QUALNAME
            && descrip != DESC_DBNAME)
        {
            if (quantity_in_words)
                buff << number_in_words(this->quantity) << " ";
            else
                buff << this->quantity << " ";
        }
    }
    else
    {
        switch (descrip)
        {
        case DESC_CAP_THE:    buff << "The "; break;
        case DESC_NOCAP_THE:  buff << "the "; break;
        case DESC_CAP_A:      buff << (startvowel ? "An " : "A "); break;

        case DESC_CAP_YOUR:   buff << "Your "; break;
        case DESC_NOCAP_YOUR: buff << "your "; break;
        case DESC_NOCAP_ITS:  buff << "its "; break;

        case DESC_NOCAP_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
                              buff << (startvowel ? "an " : "a "); break;

        case DESC_PLAIN:
        default:
            break;
        }
    }

    buff << auxname;

    bool equipped = false;
    if (descrip == DESC_INVENTORY_EQUIP && item_is_equipped(*this, true))
    {
        ASSERT( this->link != -1 );
        equipped = true;

        if (!you_tran_can_wear(*this) && you.equip[EQ_WEAPON] != this->link
            && this->link != you.m_quiver->get_fire_item())
        {
            buff << " (melded)";
        }
        else if (this->link == you.equip[EQ_WEAPON])
        {
            if (this->base_type == OBJ_WEAPONS || item_is_staff(*this))
                buff << " (weapon)";
            else
                buff << " (in hand)";
        }
        else if (this->link == you.equip[EQ_CLOAK]
                 || this->link == you.equip[EQ_HELMET]
                 || this->link == you.equip[EQ_GLOVES]
                 || this->link == you.equip[EQ_BOOTS]
                 || this->link == you.equip[EQ_SHIELD]
                 || this->link == you.equip[EQ_BODY_ARMOUR])
        {
            buff << " (worn)";
        }
        else if (this->link == you.equip[EQ_LEFT_RING])
        {
            buff << " (left hand)";
        }
        else if (this->link == you.equip[EQ_RIGHT_RING])
        {
            buff << " (right hand)";
        }
        else if (this->link == you.equip[EQ_AMULET])
        {
            buff << " (around neck)";
        }
        else if (this->link == you.m_quiver->get_fire_item())
        {
            buff << " (quivered)";
        }
    }

    if (descrip != DESC_BASENAME)
    {
        const bool  tried  =  !ident && !equipped && item_type_tried(*this);
        std::string tried_str;

        if (tried)
        {
            item_type_id_state_type id_type = get_ident_type(*this);

            if (id_type == ID_MON_TRIED_TYPE)
                tried_str = "tried by monster";
            else if (id_type == ID_TRIED_ITEM_TYPE)
                tried_str = "tried on item";
            else
                tried_str = "tried";
        }

        if ( with_inscription && !(this->inscription.empty()) )
        {
            buff << " {";
            if (tried)
                buff << tried_str << ", ";
            buff << this->inscription << "}";
        }
        else if (tried)
            buff << " {" << tried_str << "}";
    }

    return buff.str();
}


const char* weapon_brand_name(const item_def& item, bool terse)
{
    switch (get_weapon_brand(item))
    {
    case SPWPN_NORMAL: return "";
    case SPWPN_FLAMING: return ((terse) ? " (flame)" : " of flaming");
    case SPWPN_FREEZING: return ((terse) ? " (freeze)" : " of freezing");
    case SPWPN_HOLY_WRATH: return ((terse) ? " (holy)" : " of holy wrath");
    case SPWPN_ELECTROCUTION: return ((terse) ? " (elec)":" of electrocution");
    case SPWPN_ORC_SLAYING: return ((terse) ? " (slay orc)":" of orc slaying");
    case SPWPN_DRAGON_SLAYING: return ((terse) ? " (slay drac)":" of dragon slaying");
    case SPWPN_VENOM: return ((terse) ? " (venom)" : " of venom");
    case SPWPN_PROTECTION: return ((terse) ? " (protect)" : " of protection");
    case SPWPN_DRAINING: return ((terse) ? " (drain)" : " of draining");
    case SPWPN_SPEED: return ((terse) ? " (speed)" : " of speed");
    case SPWPN_PAIN: return ((terse) ? " (pain)" : " of pain");
    case SPWPN_DISTORTION: return ((terse) ? " (distort)" : " of distortion");
    case SPWPN_REACHING: return ((terse) ? " (reach)" : " of reaching");
    case SPWPN_RETURNING: return ((terse) ? " (return)" : " of returning");

    case SPWPN_VAMPIRICISM:
        return ((terse) ? " (vamp)" : ""); // non-terse already handled

    case SPWPN_VORPAL:
        if (is_range_weapon(item))
            return ((terse) ? " (velocity)" : " of velocity");
        else
        {
            switch (get_vorpal_type(item))
            {
            case DVORP_CRUSHING: return ((terse) ? " (crush)" :" of crushing");
            case DVORP_SLICING:  return ((terse) ? " (slice)" : " of slicing");
            case DVORP_PIERCING: return ((terse) ? " (pierce)":" of piercing");
            case DVORP_CHOPPING: return ((terse) ? " (chop)" : " of chopping");
            case DVORP_SLASHING: return ((terse) ? " (slash)" :" of slashing");
            case DVORP_STABBING: return ((terse) ? " (stab)" : " of stabbing");
            default:             return "";
            }
        }

    // ranged weapon brands
    case SPWPN_FLAME: return ((terse) ? " (flame)" : " of flame");
    case SPWPN_FROST: return ((terse) ? " (frost)" : " of frost");
    case SPWPN_PENETRATION: return ((terse) ? " (penet)" : " of penetration");
    case SPWPN_SHADOW: return ((terse) ? " (shadow)" : " of shadows");

    // both ranged and non-ranged
    case SPWPN_CHAOS: return ((terse) ? " (chaos)" : " of chaos");

    // randart brands
    default: return "";
    }
}


static const char* armour_ego_name( special_armour_type sparm, bool terse )
{
    if (!terse)
    {
        switch ( sparm )
        {
        case SPARM_RUNNING:           return "running";
        case SPARM_FIRE_RESISTANCE:   return "fire resistance";
        case SPARM_COLD_RESISTANCE:   return "cold resistance";
        case SPARM_POISON_RESISTANCE: return "poison resistance";
        case SPARM_SEE_INVISIBLE:     return "see invisible";
        case SPARM_DARKNESS:          return "darkness";
        case SPARM_STRENGTH:          return "strength";
        case SPARM_DEXTERITY:         return "dexterity";
        case SPARM_INTELLIGENCE:      return "intelligence";
        case SPARM_PONDEROUSNESS:     return "ponderousness";
        case SPARM_LEVITATION:        return "levitation";
        case SPARM_MAGIC_RESISTANCE:  return "magic resistance";
        case SPARM_PROTECTION:        return "protection";
        case SPARM_STEALTH:           return "stealth";
        case SPARM_RESISTANCE:        return "resistance";
        case SPARM_POSITIVE_ENERGY:   return "positive energy";
        case SPARM_ARCHMAGI:          return "the Archmagi";
        case SPARM_PRESERVATION:      return "preservation";
        case SPARM_REFLECTION:        return "reflection";
        default:                      return "bugginess";
        }
    }
    else
    {
        switch (sparm)
        {
        case SPARM_RUNNING:           return " {run}";
        case SPARM_FIRE_RESISTANCE:   return " {rF+}";
        case SPARM_COLD_RESISTANCE:   return " {rC+}";
        case SPARM_POISON_RESISTANCE: return " {rPois}";
        case SPARM_SEE_INVISIBLE:     return " {SInv}";
        case SPARM_DARKNESS:          return " {darkness}";
        case SPARM_STRENGTH:          return " {Str+3}";
        case SPARM_DEXTERITY:         return " {Dex+3}";
        case SPARM_INTELLIGENCE:      return " {Int+3}";
        case SPARM_PONDEROUSNESS:     return " {ponderous}";
        case SPARM_LEVITATION:        return " {Lev}";
        case SPARM_MAGIC_RESISTANCE:  return " {MR}";
        case SPARM_PROTECTION:        return " {AC+3}";
        case SPARM_STEALTH:           return " {Stlth+}";
        case SPARM_RESISTANCE:        return " {rC+ rF+}";
        case SPARM_POSITIVE_ENERGY:   return " {rN+}";
        case SPARM_ARCHMAGI:          return " {Archmagi}";
        case SPARM_PRESERVATION:      return " {rCorr, Cons}";
        case SPARM_REFLECTION:        return " {rflct}";
        default:                      return " {buggy}";
        }
    }
}

const char* wand_type_name(int wandtype)
{
    switch (static_cast<wand_type>(wandtype))
    {
    case WAND_FLAME:           return "flame";
    case WAND_FROST:           return "frost";
    case WAND_SLOWING:         return "slowing";
    case WAND_HASTING:         return "hasting";
    case WAND_MAGIC_DARTS:     return "magic darts";
    case WAND_HEALING:         return "healing";
    case WAND_PARALYSIS:       return "paralysis";
    case WAND_FIRE:            return "fire";
    case WAND_COLD:            return "cold";
    case WAND_CONFUSION:       return "confusion";
    case WAND_INVISIBILITY:    return "invisibility";
    case WAND_DIGGING:         return "digging";
    case WAND_FIREBALL:        return "fireball";
    case WAND_TELEPORTATION:   return "teleportation";
    case WAND_LIGHTNING:       return "lightning";
    case WAND_POLYMORPH_OTHER: return "polymorph other";
    case WAND_ENSLAVEMENT:     return "enslavement";
    case WAND_DRAINING:        return "draining";
    case WAND_RANDOM_EFFECTS:  return "random effects";
    case WAND_DISINTEGRATION:  return "disintegration";
    default:                   return "bugginess";
    }
}

static const char* wand_secondary_string(int s)
{
    switch (s)
    {
    case 0:  return "";
    case 1:  return "jewelled ";
    case 2:  return "curved ";
    case 3:  return "long ";
    case 4:  return "short ";
    case 5:  return "twisted ";
    case 6:  return "crooked ";
    case 7:  return "forked ";
    case 8:  return "shiny ";
    case 9:  return "blackened ";
    case 10: return "tapered ";
    case 11: return "glowing ";
    case 12: return "worn ";
    case 13: return "encrusted ";
    case 14: return "runed ";
    case 15: return "sharpened ";
    default: return "buggily ";
    }
}

static const char* wand_primary_string(int p)
{
    switch (p)
    {
    case 0:  return "iron";
    case 1:  return "brass";
    case 2:  return "bone";
    case 3:  return "wooden";
    case 4:  return "copper";
    case 5:  return "gold";
    case 6:  return "silver";
    case 7:  return "bronze";
    case 8:  return "ivory";
    case 9:  return "glass";
    case 10: return "lead";
    case 11: return "fluorescent";
    default: return "buggy";
    }
}

static const char* potion_type_name(int potiontype)
{
    switch ( static_cast<potion_type>(potiontype) )
    {
    case POT_HEALING:           return "healing";
    case POT_HEAL_WOUNDS:       return "heal wounds";
    case POT_SPEED:             return "speed";
    case POT_MIGHT:             return "might";
    case POT_GAIN_STRENGTH:     return "gain strength";
    case POT_GAIN_DEXTERITY:    return "gain dexterity";
    case POT_GAIN_INTELLIGENCE: return "gain intelligence";
    case POT_LEVITATION:        return "levitation";
    case POT_POISON:            return "poison";
    case POT_SLOWING:           return "slowing";
    case POT_PARALYSIS:         return "paralysis";
    case POT_CONFUSION:         return "confusion";
    case POT_INVISIBILITY:      return "invisibility";
    case POT_PORRIDGE:          return "porridge";
    case POT_DEGENERATION:      return "degeneration";
    case POT_DECAY:             return "decay";
    case POT_WATER:             return "water";
    case POT_EXPERIENCE:        return "experience";
    case POT_MAGIC:             return "magic";
    case POT_RESTORE_ABILITIES: return "restore abilities";
    case POT_STRONG_POISON:     return "strong poison";
    case POT_BERSERK_RAGE:      return "berserk rage";
    case POT_CURE_MUTATION:     return "cure mutation";
    case POT_MUTATION:          return "mutation";
    case POT_BLOOD:             return "blood";
    case POT_BLOOD_COAGULATED:  return "coagulated blood";
    case POT_RESISTANCE:        return "resistance";
    default:                    return "bugginess";
    }
}

static const char* scroll_type_name(int scrolltype)
{
    switch ( static_cast<scroll_type>(scrolltype) )
    {
    case SCR_IDENTIFY:           return "identify";
    case SCR_TELEPORTATION:      return "teleportation";
    case SCR_FEAR:               return "fear";
    case SCR_NOISE:              return "noise";
    case SCR_REMOVE_CURSE:       return "remove curse";
    case SCR_DETECT_CURSE:       return "detect curse";
    case SCR_SUMMONING:          return "summoning";
    case SCR_ENCHANT_WEAPON_I:   return "enchant weapon I";
    case SCR_ENCHANT_ARMOUR:     return "enchant armour";
    case SCR_TORMENT:            return "torment";
    case SCR_RANDOM_USELESSNESS: return "random uselessness";
    case SCR_CURSE_WEAPON:       return "curse weapon";
    case SCR_CURSE_ARMOUR:       return "curse armour";
    case SCR_IMMOLATION:         return "immolation";
    case SCR_BLINKING:           return "blinking";
    case SCR_PAPER:              return "paper";
    case SCR_MAGIC_MAPPING:      return "magic mapping";
    case SCR_FOG:                return "fog";
    case SCR_ACQUIREMENT:        return "acquirement";
    case SCR_ENCHANT_WEAPON_II:  return "enchant weapon II";
    case SCR_VORPALISE_WEAPON:   return "vorpalise weapon";
    case SCR_RECHARGING:         return "recharging";
    case SCR_ENCHANT_WEAPON_III: return "enchant weapon III";
    case SCR_HOLY_WORD:          return "holy word";
    case SCR_VULNERABILITY:      return "vulnerability";
    default:                     return "bugginess";
    }
}

static const char* jewellery_type_name(int jeweltype)
{
    switch (static_cast<jewellery_type>(jeweltype))
    {
    case RING_REGENERATION:          return "ring of regeneration";
    case RING_PROTECTION:            return "ring of protection";
    case RING_PROTECTION_FROM_FIRE:  return "ring of protection from fire";
    case RING_POISON_RESISTANCE:     return "ring of poison resistance";
    case RING_PROTECTION_FROM_COLD:  return "ring of protection from cold";
    case RING_STRENGTH:              return "ring of strength";
    case RING_SLAYING:               return "ring of slaying";
    case RING_SEE_INVISIBLE:         return "ring of see invisible";
    case RING_INVISIBILITY:          return "ring of invisibility";
    case RING_HUNGER:                return "ring of hunger";
    case RING_TELEPORTATION:         return "ring of teleportation";
    case RING_EVASION:               return "ring of evasion";
    case RING_SUSTAIN_ABILITIES:     return "ring of sustain abilities";
    case RING_SUSTENANCE:            return "ring of sustenance";
    case RING_DEXTERITY:             return "ring of dexterity";
    case RING_INTELLIGENCE:          return "ring of intelligence";
    case RING_WIZARDRY:              return "ring of wizardry";
    case RING_MAGICAL_POWER:         return "ring of magical power";
    case RING_LEVITATION:            return "ring of levitation";
    case RING_LIFE_PROTECTION:       return "ring of life protection";
    case RING_PROTECTION_FROM_MAGIC: return "ring of protection from magic";
    case RING_FIRE:                  return "ring of fire";
    case RING_ICE:                   return "ring of ice";
    case RING_TELEPORT_CONTROL:      return "ring of teleport control";
    case AMU_RAGE:              return "amulet of rage";
    case AMU_RESIST_SLOW:       return "amulet of resist slowing";
    case AMU_CLARITY:           return "amulet of clarity";
    case AMU_WARDING:           return "amulet of warding";
    case AMU_RESIST_CORROSION:  return "amulet of resist corrosion";
    case AMU_THE_GOURMAND:      return "amulet of the gourmand";
    case AMU_CONSERVATION:      return "amulet of conservation";
    case AMU_CONTROLLED_FLIGHT: return "amulet of controlled flight";
    case AMU_INACCURACY:        return "amulet of inaccuracy";
    case AMU_RESIST_MUTATION:   return "amulet of resist mutation";
    default: return "buggy jewellery";
    }
}

static const char* ring_secondary_string(int s)
{
    switch (s)
    {
    case 1:  return "encrusted ";
    case 2:  return "glowing ";
    case 3:  return "tubular ";
    case 4:  return "runed ";
    case 5:  return "blackened ";
    case 6:  return "scratched ";
    case 7:  return "small ";
    case 8:  return "large ";
    case 9:  return "twisted ";
    case 10: return "shiny ";
    case 11: return "notched ";
    case 12: return "knobbly ";
    default: return "";
    }
}

static const char* ring_primary_string(int p)
{
    switch (p)
    {
    case 0:  return "wooden";
    case 1:  return "silver";
    case 2:  return "golden";
    case 3:  return "iron";
    case 4:  return "steel";
    case 5:  return "bronze";
    case 6:  return "brass";
    case 7:  return "copper";
    case 8:  return "granite";
    case 9:  return "ivory";
    case 10: return "bone";
    case 11: return "marble";
    case 12: return "jade";
    case 13: return "glass";
    default: return "buggy";
    }
}

static const char* amulet_secondary_string(int s)
{
    switch (s)
    {
    case 0:  return "dented ";
    case 1:  return "square ";
    case 2:  return "thick ";
    case 3:  return "thin ";
    case 4:  return "runed ";
    case 5:  return "blackened ";
    case 6:  return "glowing ";
    case 7:  return "small ";
    case 8:  return "large ";
    case 9:  return "twisted ";
    case 10: return "tiny ";
    case 11: return "triangular ";
    case 12: return "lumpy ";
    default: return "";
    }
}

static const char* amulet_primary_string(int p)
{
    switch (p)
    {
    case 0:  return "zirconium";
    case 1:  return "sapphire";
    case 2:  return "golden";
    case 3:  return "emerald";
    case 4:  return "garnet";
    case 5:  return "bronze";
    case 6:  return "brass";
    case 7:  return "copper";
    case 8:  return "ruby";
    case 9:  return "ivory";
    case 10: return "bone";
    case 11: return "platinum";
    case 12: return "jade";
    case 13: return "fluorescent";
    default: return "buggy";
    }
}

static const char* rune_type_name(int p)
{
    switch (static_cast<rune_type>(p))
    {
    case RUNE_DIS:         return "iron";
    case RUNE_GEHENNA:     return "obsidian";
    case RUNE_COCYTUS:     return "icy";
    case RUNE_TARTARUS:    return "bone";
    case RUNE_SLIME_PITS:  return "slimy";
    case RUNE_VAULTS:      return "silver";
    case RUNE_SNAKE_PIT:   return "serpentine";
    case RUNE_ELVEN_HALLS: return "elven";
    case RUNE_TOMB:        return "golden";
    case RUNE_SWAMP:       return "decaying";
    case RUNE_SHOALS:      return "barnacled";

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

static const char* deck_rarity_name(deck_rarity_type rarity)
{
    switch (rarity)
    {
    case DECK_RARITY_COMMON:    return "plain";
    case DECK_RARITY_RARE:      return "ornate";
    case DECK_RARITY_LEGENDARY: return "legendary";
    }
    return "buggy rarity";
}

static const char* misc_type_name(int type, bool known)
{
    if (known)
    {
        switch ( static_cast<misc_item_type>(type) )
        {
        case MISC_DECK_OF_ESCAPE:      return "deck of escape";
        case MISC_DECK_OF_DESTRUCTION: return "deck of destruction";
        case MISC_DECK_OF_DUNGEONS:    return "deck of dungeons";
        case MISC_DECK_OF_SUMMONING:   return "deck of summonings";
        case MISC_DECK_OF_WONDERS:     return "deck of wonders";
        case MISC_DECK_OF_PUNISHMENT:  return "deck of punishment";
        case MISC_DECK_OF_WAR:         return "deck of war";
        case MISC_DECK_OF_CHANGES:     return "deck of changes";
        case MISC_DECK_OF_DEFENCE:     return "deck of defence";

        case MISC_CRYSTAL_BALL_OF_ENERGY:   return "crystal ball of energy";
        case MISC_CRYSTAL_BALL_OF_FIXATION: return "crystal ball of fixation";
        case MISC_CRYSTAL_BALL_OF_SEEING:   return "crystal ball of seeing";
        case MISC_BOX_OF_BEASTS:            return "box of beasts";
        case MISC_EMPTY_EBONY_CASKET:       return "empty ebony casket";
        case MISC_AIR_ELEMENTAL_FAN:        return "air elemental fan";
        case MISC_LAMP_OF_FIRE:             return "lamp of fire";
        case MISC_LANTERN_OF_SHADOWS:       return "lantern of shadows";
        case MISC_HORN_OF_GERYON:           return "horn of Geryon";
        case MISC_DISC_OF_STORMS:           return "disc of storms";
        case MISC_BOTTLED_EFREET:           return "bottled efreet";
        case MISC_STONE_OF_EARTH_ELEMENTALS:
            return "stone of earth elementals";

        case MISC_RUNE_OF_ZOT:
        case NUM_MISCELLANY:
            return "buggy miscellaneous item";
        }
    }
    else
    {
        switch ( static_cast<misc_item_type>(type) )
        {
        case MISC_DECK_OF_ESCAPE:
        case MISC_DECK_OF_DESTRUCTION:
        case MISC_DECK_OF_DUNGEONS:
        case MISC_DECK_OF_SUMMONING:
        case MISC_DECK_OF_WONDERS:
        case MISC_DECK_OF_PUNISHMENT:
        case MISC_DECK_OF_WAR:
        case MISC_DECK_OF_CHANGES:
        case MISC_DECK_OF_DEFENCE:
            return "deck of cards";
        case MISC_CRYSTAL_BALL_OF_ENERGY:
        case MISC_CRYSTAL_BALL_OF_FIXATION:
        case MISC_CRYSTAL_BALL_OF_SEEING:
            return "crystal ball";
        case MISC_BOX_OF_BEASTS:
        case MISC_EMPTY_EBONY_CASKET:
            return "small ebony casket";
        case MISC_AIR_ELEMENTAL_FAN:         return "gauzy fan";
        case MISC_LAMP_OF_FIRE:              return "blazing lamp";
        case MISC_LANTERN_OF_SHADOWS:        return "bone lantern";
        case MISC_HORN_OF_GERYON:            return "silver horn";
        case MISC_DISC_OF_STORMS:            return "grey disc";
        case MISC_STONE_OF_EARTH_ELEMENTALS: return "nondescript stone";
        case MISC_BOTTLED_EFREET:            return "sealed bronze flask";

        case MISC_RUNE_OF_ZOT:
        case NUM_MISCELLANY:
            return "buggy miscellaneous item";
        }
    }
    return "very buggy miscellaneous item";
}

static const char* book_secondary_string(int s)
{
    switch (s)
    {
    case 0:  return "";
    case 1:  return "chunky ";
    case 2:  return "thick ";
    case 3:  return "thin ";
    case 4:  return "wide ";
    case 5:  return "glowing ";
    case 6:  return "dog-eared ";
    case 7:  return "oblong ";
    case 8:  return "runed ";
    case 9:  return "";
    case 10: return "";
    case 11: return "";
    default: return "buggily ";
    }
}

static const char* book_primary_string(int p)
{
    switch (p)
    {
    case 0:  return "paperback ";
    case 1:  return "hardcover ";
    case 2:  return "leatherbound ";
    case 3:  return "metal-bound ";
    case 4:  return "papyrus ";
    case 5:  return "";
    case 6:  return "";
    default: return "buggy ";
    }
}

static const char* book_type_name(int booktype)
{
    switch ( static_cast<book_type>(booktype) )
    {
    case BOOK_MINOR_MAGIC_I:
    case BOOK_MINOR_MAGIC_II:
    case BOOK_MINOR_MAGIC_III:
        return "Minor Magic";
    case BOOK_CONJURATIONS_I:
    case BOOK_CONJURATIONS_II:
        return "Conjurations";
    case BOOK_FLAMES:                 return "Flames";
    case BOOK_FROST:                  return "Frost";
    case BOOK_SUMMONINGS:             return "Summonings";
    case BOOK_FIRE:                   return "Fire";
    case BOOK_ICE:                    return "Ice";
    case BOOK_SURVEYANCES:            return "Surveyances";
    case BOOK_SPATIAL_TRANSLOCATIONS: return "Spatial Translocations";
    case BOOK_ENCHANTMENTS:           return "Enchantments";
    case BOOK_TEMPESTS:               return "the Tempests";
    case BOOK_DEATH:                  return "Death";
    case BOOK_HINDERANCE:             return "Hinderance";
    case BOOK_CHANGES:                return "Changes";
    case BOOK_TRANSFIGURATIONS:       return "Transfigurations";
    case BOOK_PRACTICAL_MAGIC:        return "Practical Magic";
    case BOOK_WAR_CHANTS:             return "War Chants";
    case BOOK_CLOUDS:                 return "Clouds";
    case BOOK_NECROMANCY:             return "Necromancy";
    case BOOK_CALLINGS:               return "Callings";
    case BOOK_CHARMS:                 return "Charms";
    case BOOK_DEMONOLOGY:             return "Demonology";
    case BOOK_AIR:                    return "Air";
    case BOOK_SKY:                    return "the Sky";
    case BOOK_DIVINATIONS:            return "Divinations";
    case BOOK_WARP:                   return "the Warp";
    case BOOK_ENVENOMATIONS:          return "Envenomations";
    case BOOK_ANNIHILATIONS:          return "Annihilations";
    case BOOK_UNLIFE:                 return "Unlife";
    case BOOK_CONTROL:                return "Control";
    case BOOK_MUTATIONS:              return "Morphology";
    case BOOK_TUKIMA:                 return "Tukima";
    case BOOK_GEOMANCY:               return "Geomancy";
    case BOOK_EARTH:                  return "the Earth";
    case BOOK_WIZARDRY:               return "Wizardry";
    case BOOK_POWER:                  return "Power";
    case BOOK_CANTRIPS:               return "Cantrips";
    case BOOK_PARTY_TRICKS:           return "Party Tricks";
    case BOOK_STALKING:               return "Stalking";
    case BOOK_RANDART_LEVEL:          return "Fixed Level";
    case BOOK_RANDART_THEME:          return "Fixed Theme";
    case BOOK_CARD_EFFECT:            return "Card Effects";
    default:                          return "Bugginess";
    }
}

static const char* staff_secondary_string(int p)
{
    switch (p) // general descriptions
    {
    case 0:  return "crooked ";
    case 1:  return "knobbly ";
    case 2:  return "weird ";
    case 3:  return "gnarled ";
    case 4:  return "thin ";
    case 5:  return "curved ";
    case 6:  return "twisted ";
    case 7:  return "thick ";
    case 8:  return "long ";
    case 9:  return "short ";
    default: return "buggily ";
    }
}

static const char* staff_primary_string(int p)
{
    switch (p) // special attributes
    {
    case 0:  return "glowing ";
    case 1:  return "jewelled ";
    case 2:  return "runed ";
    case 3:  return "smoking ";
    default: return "buggy ";
    }
}

static const char* staff_type_name(int stafftype)
{
    switch (static_cast<stave_type>(stafftype))
    {
    // staves
    case STAFF_WIZARDRY:    return "wizardry";
    case STAFF_POWER:       return "power";
    case STAFF_FIRE:        return "fire";
    case STAFF_COLD:        return "cold";
    case STAFF_POISON:      return "poison";
    case STAFF_ENERGY:      return "energy";
    case STAFF_DEATH:       return "death";
    case STAFF_CONJURATION: return "conjuration";
    case STAFF_ENCHANTMENT: return "enchantment";
    case STAFF_AIR:         return "air";
    case STAFF_EARTH:       return "earth";
    case STAFF_SUMMONING:   return "summoning";

    // rods
    case STAFF_SPELL_SUMMONING: return "summoning";
    case STAFF_CHANNELING:      return "channeling";
    case STAFF_WARDING:         return "warding";
    case STAFF_DISCOVERY:       return "discovery";
    case STAFF_SMITING:         return "smiting";
    case STAFF_STRIKING:        return "striking";
    case STAFF_DEMONOLOGY:      return "demonology";
    case STAFF_VENOM:           return "venom";

    case STAFF_DESTRUCTION_I:
    case STAFF_DESTRUCTION_II:
    case STAFF_DESTRUCTION_III:
    case STAFF_DESTRUCTION_IV:
        return "destruction";

    default: return "bugginess";
    }
}

const char* racial_description_string(const item_def& item, bool terse)
{
    switch (get_equip_race( item ))
    {
    case ISFLAG_ORCISH:
        return terse ? "orc " : "orcish ";
    case ISFLAG_ELVEN:
        return terse ? "elf " : "elven ";
    case ISFLAG_DWARVEN:
        return terse ? "dwarf " : "dwarven ";
    default:
        return "";
    }
}

// gcc (and maybe the C standard) says that if you output
// 0, even with showpos set, you get 0, not +0. This is a workaround.
static void output_with_sign(std::ostream& os, int val)
{
    if (val >= 0)
        os << '+';
    os << val;
}

// Note that "terse" is only currently used for the "in hand" listing on
// the game screen.
std::string item_def::name_aux(description_level_type desc,
                               bool terse, bool ident,
                               unsigned long ignore_flags) const
{
    // Shortcuts
    const int item_typ   = this->sub_type;
    const int it_plus    = this->plus;
    const int item_plus2 = this->plus2;

    const bool know_type = ident || item_type_known(*this);

    const bool dbname   = (desc == DESC_DBNAME);
    const bool basename = (desc == DESC_BASENAME || (dbname && !know_type));
    const bool qualname = (desc == DESC_QUALNAME);

    const bool know_curse =
        !basename && !qualname && !dbname
        && !testbits(ignore_flags, ISFLAG_KNOW_CURSE)
        && (ident || item_ident(*this, ISFLAG_KNOW_CURSE));

    const bool __know_pluses =
        !basename && !qualname && !dbname
        && !testbits(ignore_flags, ISFLAG_KNOW_PLUSES)
        && (ident || item_ident(*this, ISFLAG_KNOW_PLUSES));

    const bool know_brand =
        !basename && !qualname && !dbname
        && !testbits(ignore_flags, ISFLAG_KNOW_TYPE)
        && (ident || item_type_known(*this));

    const bool know_ego = know_brand;

    const bool know_cosmetic = !__know_pluses && !terse & !basename
        && !qualname && !dbname
        && !(ignore_flags & ISFLAG_COSMETIC_MASK);

    // So that know_cosmetic won't be affected by ignore_flags.
    const bool know_pluses = __know_pluses
        && !testbits(ignore_flags, ISFLAG_KNOW_PLUSES);

    const bool know_racial = !(ignore_flags & ISFLAG_RACIAL_MASK);

    const bool need_plural = !basename && !dbname;
    int brand;

    std::ostringstream buff;

    switch (this->base_type)
    {
    case OBJ_WEAPONS:
        if (know_curse && !terse)
        {
            // We don't bother printing "uncursed" if the item is identified
            // for pluses (its state should be obvious), this is so that
            // the weapon name is kept short (there isn't a lot of room
            // for the name on the main screen).  If you're going to change
            // this behaviour, *please* make it so that there is an option
            // that maintains this behaviour. -- bwr
            // Nor for artefacts. Again, the state should be obvious. --jpeg
            if (item_cursed(*this))
                buff << "cursed ";
            else if (Options.show_uncursed && !know_pluses
                     && (!know_type || !is_artefact(*this)))
                buff << "uncursed ";
        }

        if (know_pluses)
        {
            if (terse && it_plus == item_plus2)
                output_with_sign(buff, it_plus);
            else
            {
                output_with_sign(buff, it_plus);
                buff << ',';
                output_with_sign(buff, item_plus2);
            }
            buff << " ";
        }

        if (is_artefact(*this) && !dbname)
        {
            buff << get_artefact_name(*this);
            break;
        }

        // Now that we can have "glowing elven" weapons, it's
        // probably a good idea to cut out the descriptive
        // term once it's become obsolete. -- bwr
        if (know_cosmetic)
        {
            switch (get_equip_desc(*this))
            {
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

        if (!basename && !dbname && know_racial)
        {
            // Always give racial type (it does have game effects).
            buff << racial_description_string(*this, terse);
        }

        if (know_brand && !terse
            && get_weapon_brand(*this) == SPWPN_VAMPIRICISM)
        {
            buff << "vampiric ";
        }
        buff << item_base_name(*this);

        if (know_brand)
            buff << weapon_brand_name(*this, terse);

        if (know_curse && item_cursed(*this) && terse)
            buff << " (curse)";
        break;

    case OBJ_MISSILES:
        brand = get_ammo_brand(*this);

        if (know_brand)
        {
            switch (brand)
            {
            case SPMSL_POISONED:
                buff << ((terse) ? "poison " : "poisoned ");
                break;
            case SPMSL_CURARE:
                buff << ((terse) ? "curare " : "curare-tipped ");
                break;
            case SPMSL_EXPLODING:
                buff << ((terse) ? "explode " : "exploding ");
                break;
            case SPMSL_STEEL:
                buff << "steel ";
                break;
            case SPMSL_SILVER:
                buff << "silver ";
                break;

            default:
                break;
            }

        }

        if (know_pluses)
        {
            output_with_sign(buff, it_plus);
            buff << ' ';
        }

        if (!basename && !dbname)
            buff << racial_description_string(*this, terse);

        buff << ammo_name(static_cast<missile_type>(item_typ));

        if (know_brand)
        {
            switch (brand)
            {
            case SPMSL_FLAME:
                buff << ((terse) ? " (flame)" : " of flame");
                break;
            case SPMSL_FROST:
                buff << ((terse) ? " (frost)" : " of frost");
                break;
            case SPMSL_NORMAL:
            case SPMSL_POISONED:
            case SPMSL_CURARE:
            case SPMSL_EXPLODING:
            case SPMSL_STEEL:
            case SPMSL_SILVER:
                break;
            case SPMSL_RETURNING:
                buff << ((terse) ? " (return)" : " of returning");
                break;
            case SPMSL_CHAOS:
                buff << ((terse) ? " (chaos)" : " of chaos");
                break;
            case SPMSL_PENETRATION:
                buff << ((terse) ? " (penet)" : " of penetration");
                break;
            case SPMSL_SHADOW:
                buff << ((terse) ? " (shadow)" : " of shadows");
                break;
            case SPMSL_DISPERSAL:
                buff << ((terse) ? " (disperse)" : " of dispersal");
                break;

            default:
                buff << " (buggy)";
            }
        }
        break;

    case OBJ_ARMOUR:
        if (know_curse && !terse)
        {
            if (item_cursed(*this))
                buff << "cursed ";
            else if (Options.show_uncursed && !know_pluses)
                buff << "uncursed ";
        }

        if (know_pluses)
        {
            output_with_sign(buff, it_plus);
            buff << ' ';
        }

        if (item_typ == ARM_GLOVES || item_typ == ARM_BOOTS)
            buff << "pair of ";

        // When asking for the base item name, randartism is ignored.
        if (is_random_artefact(*this) && !basename && !dbname)
        {
            buff << get_artefact_name(*this);
            break;
        }

        // Now that we can have "glowing elven" armour, it's
        // probably a good idea to cut out the descriptive
        // term once it's become obsolete. -- bwr
        if (know_cosmetic)
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

        if (!basename && !dbname)
        {
            // always give racial description (has game effects)
            buff << racial_description_string(*this, terse);
        }

        if (!basename && !dbname && is_hard_helmet(*this))
        {
            const short dhelm = get_helmet_desc(*this);

            buff <<
                   ((dhelm == THELM_DESC_PLAIN)    ? "" :
                    (dhelm == THELM_DESC_WINGED)   ? "winged "  :
                    (dhelm == THELM_DESC_HORNED)   ? "horned "  :
                    (dhelm == THELM_DESC_CRESTED)  ? "crested " :
                    (dhelm == THELM_DESC_PLUMED)   ? "plumed "  :
                    (dhelm == THELM_DESC_SPIKED)   ? "spiked "  :
                    (dhelm == THELM_DESC_VISORED)  ? "visored " :
                    (dhelm == THELM_DESC_JEWELLED) ? "jewelled "
                                                   : "buggy ");
        }

        buff << item_base_name(*this);

        if (know_ego)
        {
            const special_armour_type sparm = get_armour_ego_type( *this );

            if (sparm != SPARM_NORMAL)
            {
                if (!terse)
                    buff << " of ";

                // "naga barding of running" doesn't make any sense, and yes,
                // they are possible.
                if (sub_type == ARM_NAGA_BARDING && sparm == SPARM_RUNNING)
                    buff << (terse ? "speed" : "speedy slithering");
                else
                    buff << armour_ego_name(sparm, terse);
            }
        }

        if (know_curse && item_cursed(*this) && terse)
            buff << " (curse)";
        break;

    case OBJ_WANDS:
        if (basename)
        {
            buff << "wand";
            break;
        }

        if (know_type)
            buff << "wand of " << wand_type_name(item_typ);
        else
        {
            buff << wand_secondary_string(this->special / 12)
                 << wand_primary_string(this->special % 12)
                 << " wand";
        }

        if (know_pluses)
            buff << " (" << it_plus << ")";
        else if (!dbname)
        {
            if (item_plus2 == ZAPCOUNT_EMPTY)
                buff << " {empty}";
            else if (item_plus2 == ZAPCOUNT_MAX_CHARGED)
                buff << " {fully recharged}";
            else if (item_plus2 == ZAPCOUNT_RECHARGED)
                buff << " {recharged}";
            else if (item_plus2 > 0)
                buff << " {zapped: " << item_plus2 << '}';
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
            const int pqual   = PQUAL(this->plus);
            const int pcolour = PCOLOUR(this->plus);

            static const char *potion_qualifiers[] = {
                "",  "bubbling ", "fuming ", "fizzy ", "viscous ", "lumpy ",
                "smoky ", "glowing ", "sedimented ", "metallic ", "murky ",
                "gluggy ", "oily ", "slimy ", "emulsified "
            };
            COMPILE_CHECK( ARRAYSZ(potion_qualifiers) == PDQ_NQUALS, c1 );

            static const char *potion_colours[] = {
                "clear", "blue", "black", "silvery", "cyan", "purple",
                "orange", "inky", "red", "yellow", "green", "brown", "pink",
                "white"
            };
            COMPILE_CHECK( ARRAYSZ(potion_colours) == PDC_NCOLOURS, c1 );

            const char *qualifier =
                (pqual < 0 || pqual >= PDQ_NQUALS)? "bug-filled "
                                    : potion_qualifiers[pqual];

            const char *clr =  (pcolour < 0 || pcolour >= PDC_NCOLOURS)?
                                   "bogus" : potion_colours[pcolour];

            buff << qualifier << clr << " potion";
        }
        break;

    case OBJ_FOOD:
        switch (item_typ)
        {
        case FOOD_MEAT_RATION: buff << "meat ration"; break;
        case FOOD_BREAD_RATION: buff << "bread ration"; break;
        case FOOD_PEAR: buff << "pear"; break;
        case FOOD_APPLE: buff << "apple"; break;
        case FOOD_CHOKO: buff << "choko"; break;
        case FOOD_HONEYCOMB: buff << "honeycomb"; break;
        case FOOD_ROYAL_JELLY: buff << "royal jelly"; break;
        case FOOD_SNOZZCUMBER: buff << "snozzcumber"; break;
        case FOOD_PIZZA: buff << "slice of pizza"; break;
        case FOOD_APRICOT: buff << "apricot"; break;
        case FOOD_ORANGE: buff << "orange"; break;
        case FOOD_BANANA: buff << "banana"; break;
        case FOOD_STRAWBERRY: buff << "strawberry"; break;
        case FOOD_RAMBUTAN: buff << "rambutan"; break;
        case FOOD_LEMON: buff << "lemon"; break;
        case FOOD_GRAPE: buff << "grape"; break;
        case FOOD_SULTANA: buff << "sultana"; break;
        case FOOD_LYCHEE: buff << "lychee"; break;
        case FOOD_BEEF_JERKY: buff << "beef jerky"; break;
        case FOOD_CHEESE: buff << "cheese"; break;
        case FOOD_SAUSAGE: buff << "sausage"; break;
        case FOOD_CHUNK:
            if (!basename && !dbname)
            {
                if (food_is_rotten(*this))
                    buff << "rotting ";

                buff << "chunk of "
                     << mons_type_name(it_plus, DESC_PLAIN)
                     << " flesh";
            }
            else
                buff << "chunk of flesh";
            break;
        }

        break;

    case OBJ_SCROLLS:
        buff << "scroll";
        if (basename)
            break;
        else
            buff << " ";

        if (know_type)
        {
            buff << "of " << scroll_type_name(item_typ);
        }
        else
        {
            const unsigned long sseed =
                this->special
                + (static_cast<unsigned long>(it_plus) << 8)
                + (static_cast<unsigned long>(OBJ_SCROLLS) << 16);
            buff << "labeled " << make_name(sseed, true);
        }
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

        const bool is_randart = is_random_artefact(*this);

        if (know_curse)
        {
            if (item_cursed(*this))
                buff << "cursed ";
            else if (Options.show_uncursed && !terse
                     && (!is_randart || !know_type)
                     && (!ring_has_pluses(*this) || !know_pluses)
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
            if (know_pluses && ring_has_pluses(*this))
            {
                output_with_sign(buff, it_plus);

                if (ring_has_pluses(*this) == 2)
                {
                    buff << ',';
                    output_with_sign(buff, item_plus2);
                }
                buff << ' ';
            }

            buff << jewellery_type_name(item_typ);
        }
        else
        {
            if (jewellery_is_amulet(*this))
            {
                buff << amulet_secondary_string(this->special / 13)
                     << amulet_primary_string(this->special % 13)
                     << " amulet";
            }
            else  // i.e., a ring
            {
                buff << ring_secondary_string(this->special / 13)
                     << ring_primary_string(this->special % 13)
                     << " ring";
            }
        }
        break;
    }
    case OBJ_MISCELLANY:
        if (item_typ == MISC_RUNE_OF_ZOT)
        {
            if (!dbname)
                buff << rune_type_name(it_plus) << " ";
            buff << "rune of Zot";
        }
        else
        {
            if (is_deck(*this))
            {
                if (basename)
                {
                    buff << "deck of cards";
                    break;
                }
                else if (bad_deck(*this))
                {
                    buff << "BUGGY deck of cards";
                    break;
                }
                if (!dbname)
                    buff << deck_rarity_name(deck_rarity(*this)) << ' ';
            }
            buff << misc_type_name(item_typ, know_type);
            if (is_deck(*this) && !dbname
                && (top_card_is_known(*this) || this->plus2 != 0))
            {
                buff << " {";
                // A marked deck!
                if (top_card_is_known(*this))
                    buff << card_name(top_card(*this));

                // How many cards have been drawn, or how many are
                // left.
                if (this->plus2 != 0)
                {
                    if (top_card_is_known(*this))
                        buff << ", ";

                    if (this->plus2 > 0)
                        buff << "drawn: ";
                    else
                        buff << "left: ";

                    buff << abs(this->plus2);
                }

                buff << "}";
            }
        }
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
            if (item_typ == BOOK_DESTRUCTION)
                buff << "ancient heavily glowing book";
            else
            {
                buff << book_secondary_string(this->special / 10)
                     << book_primary_string(this->special % 10)
                     << (item_typ == BOOK_MANUAL ? "manual" : "book");
            }
        }
        else if (item_typ == BOOK_MANUAL)
        {
            if (dbname)
                buff << "manual";
            else
                buff << "manual of " << skill_name(it_plus);
        }
        else if (item_typ == BOOK_NECRONOMICON)
            buff << "Necronomicon";
        else if (item_typ == BOOK_DESTRUCTION)
            buff << "tome of Destruction";
        else if (item_typ == BOOK_YOUNG_POISONERS)
            buff << "Young Poisoner's Handbook";
        else if (item_typ == BOOK_BEASTS)
            buff << "Monster Manual";
        else
            buff << "book of " << book_type_name(item_typ);
        break;

    case OBJ_STAVES:
        if (!know_type)
        {
            if (!basename)
            {
                buff << staff_secondary_string(this->special / 4)
                     << staff_primary_string(this->special % 4);
            }

            buff << (item_is_rod(*this) ? "rod" : "staff");
        }
        else
        {
            buff << (item_is_rod(*this) ? "rod" : "staff")
                 << " of " << staff_type_name(item_typ);
        }
        break;

    // rearranged 15 Apr 2000 {dlb}:
    case OBJ_ORBS:
        buff.str("Orb of Zot");
        break;

    case OBJ_GOLD:
        buff << "gold piece";
        break;

    // not implemented
    case OBJ_GEMSTONES:
        break;

    case OBJ_CORPSES:
    {
        if (food_is_rotten(*this) && !dbname)
            buff << "rotting ";

        const std::string _name  = get_corpse_name(*this);
        const bool        shaped = starts_with(_name, "shaped ");

        if (!dbname && !starts_with(_name, "the "))
        {
            buff << mons_type_name(it_plus, DESC_PLAIN) << ' ';

            if (!_name.empty() && shaped)
                buff << _name << ' ';
        }

        if (item_typ == CORPSE_BODY)
            buff << "corpse";
        else if (item_typ == CORPSE_SKELETON)
            buff << "skeleton";
        else
            buff << "corpse bug";

        if (!_name.empty() && !shaped)
            buff << " of " << _name;
        break;
    }

    default:
        buff << "!";
    }

    // One plural to rule them all.
    if (need_plural && this->quantity > 1 && !basename && !qualname)
        buff.str(pluralise(buff.str()));

    // Disambiguation.
    if (!terse && !basename && !dbname && know_type
        && !is_random_artefact(*this))
    {
        switch (this->base_type)
        {
        case OBJ_STAVES:
            switch (item_typ)
            {
            case STAFF_DESTRUCTION_I:
                buff << " [fire]";
                break;
            case STAFF_DESTRUCTION_II:
                buff << " [ice]";
                break;
            case STAFF_DESTRUCTION_III:
                buff << " [lightning,iron,fireball]";
                break;
            case STAFF_DESTRUCTION_IV:
                buff << " [inacc,magma,cold]";
                break;
            }
            break;

        case OBJ_BOOKS:
            switch (item_typ)
            {
            case BOOK_MINOR_MAGIC_I:
                buff << " [flame]";
                break;
            case BOOK_MINOR_MAGIC_II:
                buff << " [frost]";
                break;
            case BOOK_MINOR_MAGIC_III:
                buff << " [summ]";
                break;
            case BOOK_CONJURATIONS_I:
                buff << " [fire]";
                break;
            case BOOK_CONJURATIONS_II:
                buff << " [ice]";
                break;
            }
            break;

        default:
            break;
        }
    }

    // Rod charges.
    if (item_is_rod(*this) && know_type && know_pluses
        && !basename && !qualname && !dbname)
    {
        buff << " (" << (this->plus / ROD_CHARGE_MULT)
             << "/"  << (this->plus2 / ROD_CHARGE_MULT)
             << ")";
    }

    // debugging output -- oops, I probably block it above ... dang! {dlb}
    if (buff.str().length() < 3)
    {
        buff << "bad item (cl:" << static_cast<int>(this->base_type)
             << ",ty:" << item_typ << ",pl:" << it_plus
             << ",pl2:" << item_plus2 << ",sp:" << this->special
             << ",qu:" << this->quantity << ")";
    }

    return buff.str();
}

static item_type_id_type objtype_to_idtype(object_class_type base_type)
{
    switch (base_type)
    {
    case OBJ_WANDS:     return (IDTYPE_WANDS);
    case OBJ_SCROLLS:   return (IDTYPE_SCROLLS);
    case OBJ_JEWELLERY: return (IDTYPE_JEWELLERY);
    case OBJ_POTIONS:   return (IDTYPE_POTIONS);
    case OBJ_STAVES:    return (IDTYPE_STAVES);
    default:            return (NUM_IDTYPE);
    }
}

bool item_type_known( const item_def& item )
{
    if (item_ident(item, ISFLAG_KNOW_TYPE))
        return (true);

    // Artefacts have different descriptions from other items,
    // so we can't use general item knowledge for them.
    if (is_artefact(item))
        return (false);

    // Poisoned missiles are always identified.
    if (item.base_type == OBJ_MISSILES)
    {
        int ammo_brand = get_ammo_brand(item);
        if (ammo_brand == SPMSL_POISONED || ammo_brand == SPMSL_CURARE)
            return (true);
    }

    const item_type_id_type idt = objtype_to_idtype(item.base_type);
    if (idt != NUM_IDTYPE && item.sub_type < 50 )
        return (type_ids[idt][item.sub_type] == ID_KNOWN_TYPE);
    else
        return (false);
}

bool item_type_known(const object_class_type base_type, const int sub_type)
{
    const item_type_id_type idt = objtype_to_idtype(base_type);
    if (idt != NUM_IDTYPE && sub_type < 50 )
        return (type_ids[idt][sub_type] == ID_KNOWN_TYPE);
    else
        return (false);
}

bool item_type_tried( const item_def& item )
{
    if (item_type_known(item))
        return (false);

    if (is_artefact(item) && item.base_type == OBJ_JEWELLERY)
    {
        if (item.base_type == OBJ_JEWELLERY
            && item.props.exists("jewellery_tried")
            && item.props["jewellery_tried"].get_bool())
        {
            return (true);
        }
        return (false);
    }

    const item_type_id_type idt = objtype_to_idtype(item.base_type);
    if (idt != NUM_IDTYPE && item.sub_type < 50)
    {
        return (type_ids[idt][item.sub_type] == ID_TRIED_TYPE
                || type_ids[idt][item.sub_type] == ID_MON_TRIED_TYPE
                || type_ids[idt][item.sub_type] == ID_TRIED_ITEM_TYPE);
    }
    else
        return (false);
}

id_arr& get_typeid_array()
{
    return type_ids;
}

void set_ident_type( item_def &item, item_type_id_state_type setting,
                     bool force )
{
    if (is_artefact(item) || crawl_state.arena)
        return;

    item_type_id_state_type old_setting = get_ident_type(item);
    set_ident_type(item.base_type, item.sub_type, setting, force);

    if (setting == ID_KNOWN_TYPE && old_setting != ID_KNOWN_TYPE
        && notes_are_active() && is_interesting_item(item)
        && !(item.flags & (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET)))
    {
        // Make a note of it.
        take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_NOCAP_A).c_str(),
                       origin_desc(item).c_str()));

        // Sometimes (e.g. shops) you can ID an item before you get it;
        // don't note twice in those cases.
        item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
    }
}

void set_ident_type( object_class_type basetype, int subtype,
                     item_type_id_state_type setting, bool force )
{
    preserve_quiver_slots p;
    // Don't allow overwriting of known type with tried unless forced.
    if (!force
        && (setting == ID_MON_TRIED_TYPE || setting == ID_TRIED_TYPE)
        && setting <= get_ident_type( basetype, subtype ))
    {
        return;
    }

    const item_type_id_type idt = objtype_to_idtype(basetype);

    if (idt != NUM_IDTYPE)
    {
        if (type_ids[idt][subtype] != setting)
        {
            type_ids[idt][subtype] = setting;
            request_autoinscribe();
        }
    }
}

item_type_id_state_type get_ident_type(const item_def &item)
{
    if (is_artefact(item))
        return ID_UNKNOWN_TYPE;

    return get_ident_type(item.base_type, item.sub_type);
}

item_type_id_state_type get_ident_type(object_class_type basetype, int subtype)
{
    const item_type_id_type idt = objtype_to_idtype(basetype);
    if (idt != NUM_IDTYPE && subtype < type_ids.height())
        return type_ids[idt][subtype];
    else
        return ID_UNKNOWN_TYPE;
}

class DiscEntry : public InvEntry
{
public:
    DiscEntry(InvEntry* inv) : InvEntry(*inv->item)
    {
    }

    virtual std::string get_text() const
    {
        return std::string(" ") + item->name(DESC_PLAIN);
    }
};

static MenuEntry *discoveries_item_mangle(MenuEntry *me)
{
    InvEntry *ie = dynamic_cast<InvEntry*>(me);
    DiscEntry *newme = new DiscEntry(ie);
    delete me;

    return (newme);
}

bool item_names( const item_def *it1,
                 const item_def *it2 )
{
    return it1->name(DESC_PLAIN, false, false, false)
           < it2->name(DESC_PLAIN, false, false, false);
}

bool check_item_knowledge(bool quiet)
{
    std::vector<const item_def*> items;
    bool rc = true;

    const object_class_type idx_to_objtype[5] = { OBJ_WANDS, OBJ_SCROLLS,
                                                  OBJ_JEWELLERY, OBJ_POTIONS,
                                                  OBJ_STAVES };
    const int idx_to_maxtype[5] = { NUM_WANDS, NUM_SCROLLS,
                                    NUM_JEWELLERY, NUM_POTIONS, NUM_STAVES };


    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < idx_to_maxtype[i]; j++)
        {
            if (type_ids[i][j] == ID_KNOWN_TYPE)
            {
                item_def* ptmp = new item_def;
                if (ptmp != 0)
                {
                    ptmp->base_type = idx_to_objtype[i];
                    ptmp->sub_type  = j;
                    ptmp->colour    = 1;
                    ptmp->quantity  = 1;
                    items.push_back(ptmp);
                }
            }
        }
    }

    if (items.empty())
    {
        rc = false;
        if (!quiet)
            mpr("You don't recognise anything yet!");
    }
    else
    {
        rc = true;
        std::sort(items.begin(), items.end(), item_names);
        InvMenu menu;
        menu.set_title("You recognise:");
        menu.load_items(items, discoveries_item_mangle);
        menu.set_flags(MF_NOSELECT);
        menu.show();
        redraw_screen();

        for ( std::vector<const item_def*>::iterator iter = items.begin();
              iter != items.end(); ++iter )
            delete *iter;
    }

    return (rc);
}


// Used for: Pandemonium demonlords, shopkeepers, scrolls, random artefacts
std::string make_name( unsigned long seed, bool all_cap )
{
    char name[ITEMNAME_SIZE];
    int  numb[17];

    int i = 0;
    bool want_vowel = false;
    bool has_space = false;

    for (i = 0; i < ITEMNAME_SIZE; i++)
        name[i] = '\0';

    const int var1 = (seed & 0xFF);
    const int var2 = ((seed >>  8) & 0xFF);
    const int var3 = ((seed >> 16) & 0xFF);
    const int var4 = ((seed >> 24) & 0xFF);

    numb[0]  = 373 * var1 + 409 * var2 + 281 * var3;
    numb[1]  = 163 * var4 + 277 * var2 + 317 * var3;
    numb[2]  = 257 * var1 + 179 * var4 +  83 * var3;
    numb[3]  =  61 * var1 + 229 * var2 + 241 * var4;
    numb[4]  =  79 * var1 + 263 * var2 + 149 * var3;
    numb[5]  = 233 * var4 + 383 * var2 + 311 * var3;
    numb[6]  = 199 * var1 + 211 * var4 + 103 * var3;
    numb[7]  = 139 * var1 + 109 * var2 + 349 * var4;
    numb[8]  =  43 * var1 + 389 * var2 + 359 * var3;
    numb[9]  = 367 * var4 + 101 * var2 + 251 * var3;
    numb[10] = 293 * var1 +  59 * var4 + 151 * var3;
    numb[11] = 331 * var1 + 107 * var2 + 307 * var4;
    numb[12] =  73 * var1 + 157 * var2 + 347 * var3;
    numb[13] = 379 * var4 + 353 * var2 + 227 * var3;
    numb[14] = 181 * var1 + 173 * var4 + 193 * var3;
    numb[15] = 131 * var1 + 167 * var2 +  53 * var4;
    numb[16] = 313 * var1 + 127 * var2 + 401 * var3 + 337 * var4;

    int len = 3 + numb[0] % 5 + ((numb[1] % 5 == 0) ? numb[2] % 6 : 1);

    if (all_cap)
        len += 6;

    int j = numb[3] % 17;
    const int k = numb[4] % 17;
    int count = 0;

    for (i = 0; i < len; i++)
    {
        j = (j + 1) % 17;
        if (j == 0)
        {
            count++;
            if (count > 9)
                break;
        }

        if (!has_space && i > 5 && i < len - 4
            && (numb[(k + 10 * j) % 17] % 5) != 3)
        {
            want_vowel = true;
            name[i] = ' ';
        }
        else if (i > 0
            && (want_vowel
                || (i > 1
                    && is_random_name_vowel( name[i - 1] )
                    && !is_random_name_vowel( name[i - 2] )
                    && (numb[(k + 4 * j) % 17] % 5) <= 1 )))
        {
            want_vowel = true;
            name[i] = retvow( numb[(k + 7 * j) % 17] );

            if (is_random_name_space( name[i] ))
            {
                if (i == 0)
                {
                    want_vowel = false;
                    name[i] = retlet( numb[(k + 14 * j) % 17] );
                }
                else if (len < 7
                        || i <= 2 || i >= len - 3
                        || is_random_name_space( name[i - 1] )
                        || (i > 1 && is_random_name_space( name[i - 2] ))
                        || (i > 2
                            && !is_random_name_vowel( name[i - 1] )
                            && !is_random_name_vowel( name[i - 2] )))
                {
                    i--;
                    continue;
                }
            }
            else if (i > 1
                    && name[i] == name[i - 1]
                    && (name[i] == 'y' || name[i] == 'i'
                        || (numb[(k + 12 * j) % 17] % 5) <= 1))
            {
                i--;
                continue;
            }
        }
        else
        {
            if ((len > 3 || i != 0)
                && (numb[(k + 13 * j) % 17] % 7) <= 1
                && (i < len - 2 || (i > 0 && !is_random_name_space(name[i - 1]))))
            {
                const bool beg = ((i < 1) || is_random_name_space(name[i - 1]));
                const bool end = (i >= len - 2);

                const int first = (beg ?  0 : (end ? 14 :  0));
                const int last  = (beg ? 27 : (end ? 56 : 67));

                const int num = last - first;

                i++;

                switch (numb[(k + 11 * j) % 17] % num + first)
                {
                // start, middle
                case  0: strcat(name, "kl"); break;
                case  1: strcat(name, "gr"); break;
                case  2: strcat(name, "cl"); break;
                case  3: strcat(name, "cr"); break;
                case  4: strcat(name, "fr"); break;
                case  5: strcat(name, "pr"); break;
                case  6: strcat(name, "tr"); break;
                case  7: strcat(name, "tw"); break;
                case  8: strcat(name, "br"); break;
                case  9: strcat(name, "pl"); break;
                case 10: strcat(name, "bl"); break;
                case 11: strcat(name, "str"); i++; len++; break;
                case 12: strcat(name, "shr"); i++; len++; break;
                case 13: strcat(name, "thr"); i++; len++; break;
                // start, middle, end
                case 14: strcat(name, "sm"); break;
                case 15: strcat(name, "sh"); break;
                case 16: strcat(name, "ch"); break;
                case 17: strcat(name, "th"); break;
                case 18: strcat(name, "ph"); break;
                case 19: strcat(name, "pn"); break;
                case 20: strcat(name, "kh"); break;
                case 21: strcat(name, "gh"); break;
                case 22: strcat(name, "mn"); break;
                case 23: strcat(name, "ps"); break;
                case 24: strcat(name, "st"); break;
                case 25: strcat(name, "sk"); break;
                case 26: strcat(name, "sch"); i++; len++; break;
                // middle, end
                case 27: strcat(name, "ts"); break;
                case 28: strcat(name, "cs"); break;
                case 29: strcat(name, "xt"); break;
                case 30: strcat(name, "nt"); break;
                case 31: strcat(name, "ll"); break;
                case 32: strcat(name, "rr"); break;
                case 33: strcat(name, "ss"); break;
                case 34: strcat(name, "wk"); break;
                case 35: strcat(name, "wn"); break;
                case 36: strcat(name, "ng"); break;
                case 37: strcat(name, "cw"); break;
                case 38: strcat(name, "mp"); break;
                case 39: strcat(name, "ck"); break;
                case 40: strcat(name, "nk"); break;
                case 41: strcat(name, "dd"); break;
                case 42: strcat(name, "tt"); break;
                case 43: strcat(name, "bb"); break;
                case 44: strcat(name, "pp"); break;
                case 45: strcat(name, "nn"); break;
                case 46: strcat(name, "mm"); break;
                case 47: strcat(name, "kk"); break;
                case 48: strcat(name, "gg"); break;
                case 49: strcat(name, "ff"); break;
                case 50: strcat(name, "pt"); break;
                case 51: strcat(name, "tz"); break;
                case 52: strcat(name, "dgh"); i++; len++; break;
                case 53: strcat(name, "rgh"); i++; len++; break;
                case 54: strcat(name, "rph"); i++; len++; break;
                case 55: strcat(name, "rch"); i++; len++; break;
                // middle only
                case 56: strcat(name, "cz"); break;
                case 57: strcat(name, "xk"); break;
                case 58: strcat(name, "zx"); break;
                case 59: strcat(name, "xz"); break;
                case 60: strcat(name, "cv"); break;
                case 61: strcat(name, "vv"); break;
                case 62: strcat(name, "nl"); break;
                case 63: strcat(name, "rh"); break;
                case 64: strcat(name, "dw"); break;
                case 65: strcat(name, "nw"); break;
                case 66: strcat(name, "khl"); i++; len++; break;
                default:
                    i--;
                    break;
                }
            }
            else
            {
                if (i == 0)
                {
                    name[i] = 'a' + (numb[(k + 8 * j) % 17] % 26);
                    want_vowel = is_random_name_vowel( name[i] );
                }
                else
                {
                    name[i] = retlet( numb[(k + 3 * j) % 17] );
                }
            }
        }

        if (name[i] == '\0')
        {
            i--;
            continue;
        }

        if (want_vowel && !is_random_name_vowel( name[i] )
            || (!want_vowel && is_random_name_vowel( name[i] )))
        {
            i--;
            continue;
        }

        if (is_random_name_space( name[i] ))
            has_space = true;

        if (!is_random_name_vowel( name[i] ))
            want_vowel = true;
        else
            want_vowel = false;
    }

    // catch break and try to give a final letter
    if (i > 0
        && !is_random_name_space( name[i - 1] )
        && name[i - 1] != 'y'
        && is_random_name_vowel( name[i - 1] )
        && (count > 9 || (i < 8 && numb[16] % 3)))
    {
        name[i] = retlet( numb[j] );
    }

    len = strlen( name );

    if (len)
    {
        for (i = len - 1; i > 0; i--)
        {
            if (!isspace( name[i] ))
                break;
            else
            {
                name[i] = '\0';
                len--;
            }
        }
    }

    if (len < 4)
    {
        strcpy(name, "plog");
        len = 4;
    }

    for (i = 0; i < len; i++)
        if (all_cap || i == 0 || name[i - 1] == ' ')
            name[i] = toupper( name[i] );

    return name;
}                               // end make_name()

bool is_random_name_space(char let)
{
    return (let == ' ');
}

static bool is_random_name_vowel( char let )
{
    return (let == 'a' || let == 'e' || let == 'i' || let == 'o' || let == 'u'
            || let == 'y' || let == ' ');
}                               // end is_random_name_vowel()

static char retvow( int sed )
{
    static const char vowels[] = "aeiouaeiouaeiouy  ";
    return (vowels[ sed % (sizeof(vowels) - 1) ]);
}                               // end retvow()

static char retlet( int sed )
{
    static const char consonants[] = "bcdfghjklmnpqrstvwxzcdfghlmnrstlmnrst";
    return (consonants[ sed % (sizeof(consonants) - 1) ]);
}

bool is_interesting_item( const item_def& item )
{
    if (fully_identified(item)
        && (is_random_artefact(item)
            || is_unrandom_artefact(item)
            || is_fixed_artefact(item)))
    {
        return (true);
    }

    const std::string iname = menu_colour_item_prefix(item, false) + " "
                              + item.name(DESC_PLAIN);
    for (unsigned i = 0; i < Options.note_items.size(); ++i)
        if (Options.note_items[i].matches(iname))
            return (true);

    return (false);
}

// Returns true if an item is a potential life saver in an emergency situation.
bool is_emergency_item(const item_def &item)
{
    if (!item_type_known(item))
        return (false);

    switch (item.base_type)
    {
    case OBJ_WANDS:
        switch (item.sub_type)
        {
        case WAND_HASTING:
        case WAND_HEALING:
        case WAND_TELEPORTATION:
            return (true);
        default:
            return (false);
        }
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
        case SCR_TELEPORTATION:
        case SCR_BLINKING:
        case SCR_FEAR:
            return (true);
        default:
            return (false);
        }
    case OBJ_POTIONS:
        if (you.species == SP_MUMMY)
            return (false);

        switch (item.sub_type)
        {
        case POT_SPEED:
        case POT_HEALING:
        case POT_HEAL_WOUNDS:
        case POT_RESISTANCE:
            return (true);
        default:
            return (false);
        }
    default:
        return (false);
    }
}

// Returns true if an item can be considered particularly good.
bool is_good_item(const item_def &item)
{
    if (!item_type_known(item))
        return (false);

    if (is_emergency_item(item))
        return (true);

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        return (item.sub_type == SCR_ACQUIREMENT);
    case OBJ_POTIONS:
        switch (item.sub_type)
        {
        case POT_CURE_MUTATION:
        case POT_GAIN_STRENGTH:
        case POT_GAIN_INTELLIGENCE:
        case POT_GAIN_DEXTERITY:
        case POT_EXPERIENCE:
        case POT_MAGIC:
        case POT_BERSERK_RAGE:
        case POT_MIGHT:
        case POT_RESTORE_ABILITIES:
            return (true);
        default:
            return (false);
        }
    default:
        return (false);
    }
}

// Returns true if using an item only has harmful effects.
bool is_bad_item(const item_def &item, bool temp)
{
    if (!item_type_known(item))
        return (false);

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
        case SCR_CURSE_ARMOUR:
        case SCR_CURSE_WEAPON:
            return (true);
        case SCR_SUMMONING:
            // Summoning will always produce hostile monsters if you worship
            // a good god. (Use temp to allow autopickup to prevent monsters
            // from reading it.)
            return (temp && is_good_god(you.religion));
        default:
            return (false);
        }
    case OBJ_POTIONS:
        // Can't be bad if you can't use them.
        if (you.species == SP_MUMMY)
            return (false);

        switch (item.sub_type)
        {
        case POT_CONFUSION:
        case POT_SLOWING:
        case POT_DEGENERATION:
        case POT_DECAY:
        case POT_PARALYSIS:
            return (true);
        case POT_POISON:
        case POT_STRONG_POISON:
            // Poison is not that bad if you're poison resistant.
            return (!player_res_poison(false)
                    || !temp && you.species == SP_VAMPIRE);
        case POT_MUTATION:
            return (you.is_undead
                    && (temp || you.species != SP_VAMPIRE
                        || you.hunger_state < HS_SATIATED));
        default:
            return (false);
        }
    case OBJ_JEWELLERY:
        switch (item.sub_type)
        {
        case AMU_INACCURACY:
            return (true);
        case RING_HUNGER:
            // Even Vampires can use this ring.
            return (!you.is_undead);
        default:
            return (false);
        }
    case OBJ_MISCELLANY:
        return (item.sub_type == MISC_CRYSTAL_BALL_OF_FIXATION);

    default:
        return (false);
    }
}

// Returns true if using an item is risky but may occasionally be worthwhile.
bool is_dangerous_item(const item_def &item, bool temp)
{
    if (!item_type_known(item))
    {
        // Use-IDing these is extremely dangerous!
        if (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_CRYSTAL_BALL_OF_SEEING
                || item.sub_type == MISC_CRYSTAL_BALL_OF_ENERGY
                || item.sub_type == MISC_CRYSTAL_BALL_OF_FIXATION))
        {
            return (true);
        }
        return (false);
    }

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        if (!item_type_known(item))
            return (false);

        switch (item.sub_type)
        {
        case SCR_IMMOLATION:
            return (true);
        case SCR_TORMENT:
            return (!player_mutation_level(MUT_TORMENT_RESISTANCE)
                    || !temp && you.species == SP_VAMPIRE);
        default:
            return (false);
        }

    case OBJ_POTIONS:
        if (!item_type_known(item))
            return (false);

        switch (item.sub_type)
        {
        case POT_MUTATION:
            // Only living characters can mutate.
            return (!you.is_undead
                    || temp && you.species == SP_VAMPIRE
                       && you.hunger_state >= HS_SATIATED);
        default:
            return (false);
        }

    case OBJ_BOOKS:
        // The Tome of Destruction is certainly risky.
        return (item.sub_type == BOOK_DESTRUCTION);

    default:
        return (false);
    }
}

bool is_useless_item(const item_def &item, bool temp)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (!item_type_known(item))
            return (false);

        switch (get_weapon_brand(item))
        {
        case SPWPN_HOLY_WRATH:
            return (you.is_undead);
        case SPWPN_PAIN:
            return (temp && !you.skills[SK_NECROMANCY] && !is_artefact(item));
        }
        return (false);

    case OBJ_ARMOUR:
        return (!can_wear_armour(item, false, true));

    case OBJ_SCROLLS:
        if (!item_type_known(item))
            return (false);

        // A bad item is always useless.
        if (is_bad_item(item, temp))
            return (true);

        switch (item.sub_type)
        {
        case SCR_PAPER:
        case SCR_RANDOM_USELESSNESS:
        case SCR_NOISE:
            return (true);
        case SCR_TORMENT:
            return (player_mutation_level(MUT_TORMENT_RESISTANCE)
                    || !temp && you.species == SP_VAMPIRE);
        default:
            return (false);
        }
    case OBJ_WANDS:
        return (item.plus2 == ZAPCOUNT_EMPTY);

    case OBJ_POTIONS:
    {
        // No potion is useless if it can be used for Evaporate.
        if (player_knows_spell(SPELL_EVAPORATE))
            return (false);

        // Apart from Evaporate, mummies can't use potions.
        if (you.species == SP_MUMMY)
            return (true);

        if (!item_type_known(item))
            return (false);

        // A bad item is always useless.
        if (is_bad_item(item, temp))
            return (true);

        switch (item.sub_type)
        {
        case POT_BERSERK_RAGE:
        case POT_CURE_MUTATION:
        case POT_GAIN_STRENGTH:
        case POT_GAIN_INTELLIGENCE:
        case POT_GAIN_DEXTERITY:
            return (you.species == SP_GHOUL
                    || temp && you.species == SP_VAMPIRE
                            && you.hunger_state < HS_SATIATED);

        case POT_LEVITATION:
            return (you.permanent_levitation() || you.permanent_flight());

        case POT_PORRIDGE:
        case POT_WATER:
        case POT_BLOOD:
        case POT_BLOOD_COAGULATED:
            return (!can_ingest(item.base_type, item.sub_type, true, true,
                                                               false));
        case POT_POISON:
        case POT_STRONG_POISON:
            // If you're poison resistant, poison is only useless.
            return player_res_poison(false);
        }

        return (false);
    }
    case OBJ_JEWELLERY:
        if (!item_type_known(item))
            return (false);

        // Potentially useful.
        if (is_artefact(item))
            return (false);

        if (is_bad_item(item, temp))
            return (true);

        switch (item.sub_type)
        {
        case AMU_RAGE:
            return (you.is_undead
                        && (!temp || you.species == SP_VAMPIRE
                                     && you.hunger_state <= HS_SATIATED)
                    || you.religion == GOD_TROG);

        case RING_LIFE_PROTECTION:
            return (player_prot_life(false, temp, false) == 3);

        case RING_HUNGER:
        case RING_SUSTENANCE:
            return (you.species == SP_MUMMY
                    || temp && you.species == SP_VAMPIRE
                       && you.hunger_state == HS_STARVING);

        case RING_REGENERATION:
            return ((player_mutation_level(MUT_SLOW_HEALING) == 3)
                    || temp && you.species == SP_VAMPIRE
                       && you.hunger_state == HS_STARVING);

        case RING_SEE_INVISIBLE:
            return (player_mutation_level(MUT_ACUTE_VISION));

        case RING_POISON_RESISTANCE:
            return (player_res_poison(false, temp, false)
                    && (temp || you.species != SP_VAMPIRE));

        case AMU_CONTROLLED_FLIGHT:
            return (player_genus(GENPC_DRACONIAN) || you.permanent_flight());

        case RING_WIZARDRY:
            return (you.religion == GOD_TROG);

        case RING_TELEPORT_CONTROL:
            return (player_control_teleport(true, temp, false));

        default:
            return (false);
        }

    case OBJ_STAVES:
        if (you.religion == GOD_TROG && !item_is_rod(item))
            return (true);
        break;

    case OBJ_FOOD:
        if (!is_inedible(item))
            return (false);

        if (item.sub_type == FOOD_CHUNK
            && (you.has_spell(SPELL_SUBLIMATION_OF_BLOOD)
                || you.has_spell(SPELL_SIMULACRUM)))
        {
            return (false);
        }
        return (true);

    case OBJ_CORPSES:
        if (item.sub_type != CORPSE_SKELETON)
            return (false);

        if (you.has_spell(SPELL_BONE_SHARDS)
            || you.has_spell(SPELL_ANIMATE_DEAD)
            || you.has_spell(SPELL_ANIMATE_SKELETON)
            || player_mutation_level(MUT_RAISE_DEAD)
            || you.religion == GOD_YREDELEMNUL
               && you.piety >= piety_breakpoint(0))
        {
            return (false);
        }

        return (true);

    default:
        return (false);
    }
    return (false);
}

static const std::string _item_prefix(const item_def &item, bool temp,
                                      bool filter)
{
    std::vector<std::string> prefixes;

    // No identified/unidentified for filtering, since the user might
    // want to filter on "ident" to find scrolls of identify.
    if (filter)
        ;
    else if (item_ident(item, ISFLAG_KNOW_TYPE))
        prefixes.push_back("identified");
    else
    {
        if (get_ident_type(item) == ID_KNOWN_TYPE)
        {
            // Wands are only fully identified if we know the
            // number of charges.
            if (item.base_type == OBJ_WANDS)
                prefixes.push_back("known");

            // Rings are fully identified simply by knowing their
            // type, unless the ring has plusses, like a ring of
            // dexterity.
            else if (item.base_type == OBJ_JEWELLERY
                     && !jewellery_is_amulet(item))
            {
                if (item.plus == 0 && item.plus2 == 0)
                    prefixes.push_back("identified");
                else
                    prefixes.push_back("known");
            }
            // All other types of magical items are fully identified
            // simply by knowing the type
            else
                prefixes.push_back("identified");
        }
        else
            prefixes.push_back("unidentified");
    }

    if (good_god_hates_item_handling(item) || god_hates_item_handling(item))
        prefixes.push_back("evil_item");

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
        if (is_forbidden_food(item))
            prefixes.push_back("evil_eating");

        if (is_inedible(item))
            prefixes.push_back("inedible");
        else if (is_preferred_food(item))
            prefixes.push_back("preferred");

        // Don't include these for filtering, since the user might want
        // to use "muta" to search for "potion of cure mutation", and
        // similar.
        if (filter)
            ;
        else if (is_poisonous(item))
            prefixes.push_back("poisonous");
        else if (is_mutagenic(item))
            prefixes.push_back("mutagenic");
        else if (is_contaminated(item))
            prefixes.push_back("contaminated");
        else if (causes_rot(item))
            prefixes.push_back("rot-inducing");
        break;

    case OBJ_POTIONS:
        if (is_good_god(you.religion) && item_type_known(item)
            && is_blood_potion(item))
        {
            prefixes.push_back("evil_eating");
        }
        if (is_preferred_food(item))
            prefixes.push_back("preferred");
        break;

    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
        if (item_is_equipped(item, true))
            prefixes.push_back("equipped");
        if (is_artefact(item))
            prefixes.push_back("artefact");
        break;

    case OBJ_MISSILES:
        if (item_is_equipped(item, true))
            prefixes.push_back("equipped");
        break;

    default:
        break;
    }

    if (Options.menu_colour_prefix_class && !filter)
        prefixes.push_back(item_class_name(item.base_type, true));

    std::string result = comma_separated_line(prefixes.begin(), prefixes.end(),
                                              " ", " ");

    return result;
}

std::string menu_colour_item_prefix(const item_def &item, bool temp)
{
    return _item_prefix(item, temp, false);
}

std::string filtering_item_prefix(const item_def &item, bool temp)
{
    return _item_prefix(item, temp, true);
}

std::string get_menu_colour_prefix_tags(const item_def &item,
                                        description_level_type desc)
{
    std::string cprf       = menu_colour_item_prefix(item);
    std::string colour     = "";
    std::string colour_off = "";
    std::string item_name  = item.name(desc);
    int col = menu_colour(item_name, cprf, "pickup");

    if (col != -1)
        colour = colour_to_str( col );

    if (!colour.empty())
    {
        // Order is important here.
        colour_off  = "</" + colour + ">";
        colour      = "<" + colour + ">";
        item_name = colour + item_name + colour_off;
    }

    return (item_name);
}

std::string get_message_colour_tags(const item_def &item,
                                    description_level_type desc,
                                    msg_channel_type channel)
{
    std::string cprf       = menu_colour_item_prefix(item);
    std::string colour     = "";
    std::string colour_off = "";
    std::string item_name  = item.name(desc);
    cprf += " " + item_name;

    int col = -1;
    const std::vector<message_colour_mapping>& mcm
               = Options.message_colour_mappings;
    typedef std::vector<message_colour_mapping>::const_iterator mcmci;

    for (mcmci ci = mcm.begin(); ci != mcm.end(); ++ci)
    {
        if (ci->message.is_filtered(channel, cprf))
        {
            col = ci->colour;
            break;
        }
    }

    if (col != -1)
        colour = colour_to_str( col );

    if (!colour.empty())
    {
        // Order is important here.
        colour_off  = "</" + colour + ">";
        colour      = "<" + colour + ">";
        item_name   = colour + item_name + colour_off;
    }

    return (item_name);
}

typedef std::map<std::string, item_types_pair> item_names_map;
static item_names_map item_names_cache;

typedef std::map<unsigned, std::vector<std::string> > item_names_by_glyph_map;
static item_names_by_glyph_map item_names_by_glyph_cache;

void init_item_name_cache()
{
    const int sub_type_limits[] = {
        NUM_WEAPONS,
        NUM_MISSILES,
        NUM_ARMOURS,
        NUM_WANDS,
        NUM_FOODS,
        0, // Unknown I
        NUM_SCROLLS,
        NUM_JEWELLERY,
        NUM_POTIONS,
        0, // Unknown II
        NUM_BOOKS,
        NUM_STAVES,
        1, // Orbs
        NUM_MISCELLANY,
        0, // Corpses
        1, // Gold
        -1
    };

    item_names_cache.clear();
    item_names_by_glyph_cache.clear();

    for (int i = 0; sub_type_limits[i] != -1; i++)
    {
        object_class_type base_type = static_cast<object_class_type>(i);
        unsigned char     num_sub_types = (unsigned char) sub_type_limits[i];

        for (unsigned char sub_type = 0; sub_type < num_sub_types; sub_type++)
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

            int o = items(0, base_type, sub_type, true, 1,
                          MAKE_ITEM_NO_RACE);

            if (o == NON_ITEM)
                continue;

            item_def       &item(mitm[o]);
            item_types_pair pair = {base_type, sub_type};

            // Make sure item isn't an artefact.
            item.flags  &= ~ISFLAG_ARTEFACT_MASK;
            item.special = 0;

            std::string    name = item.name(DESC_DBNAME, true, true);
            unsigned       glyph;
            unsigned short colour;
            get_item_glyph(&item, &glyph, &colour);
            destroy_item(o, true);
            lowercase(name);

            if (base_type == OBJ_JEWELLERY && name == "buggy jewellery")
                continue;
            else if (name.find("buggy") != std::string::npos)
            {
                crawl_state.add_startup_error("Bad name for item name "
                                              " cache: " + name);
                continue;
            }

            if (item_names_cache.find(name) == item_names_cache.end())
            {
                item_names_cache[name] = pair;
                if (glyph)
                    item_names_by_glyph_cache[glyph].push_back(name);
            }
        }
    }

    ASSERT(item_names_cache.size() > 0);
}

item_types_pair item_types_by_name(std::string name)
{
    lowercase(name);

    item_names_map::iterator i = item_names_cache.find(name);

    if (i != item_names_cache.end())
        return (i->second);

    item_types_pair err = {OBJ_UNASSIGNED, 0};

    return (err);
}

std::vector<std::string> item_name_list_for_glyph(unsigned glyph)
{
    item_names_by_glyph_map::iterator i;
    i = item_names_by_glyph_cache.find(glyph);

    if (i != item_names_by_glyph_cache.end())
        return (i->second);

    std::vector<std::string> empty;
    return empty;
}

bool is_named_corpse(const item_def &corpse)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);

    return (corpse.props.exists(CORPSE_NAME_KEY));
}

std::string get_corpse_name(const item_def &corpse)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);

    if (!corpse.props.exists(CORPSE_NAME_KEY))
        return ("");

    return (corpse.props[CORPSE_NAME_KEY].get_string());
}

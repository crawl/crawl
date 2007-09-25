/*
 *  File:       itemname.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <4>      9/09/99        BWR             Added hands_required function
 *      <3>      6/13/99        BWR             Upped the base AC for heavy armour
 *      <2>      5/20/99        BWR             Extended screen lines support
 *      <1>      -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "itemname.h"

#include <sstream>
#include <iomanip>
#include <ctype.h>
#include <string.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "decks.h"
#include "food.h"
#include "invent.h"
#include "it_use2.h"
#include "itemprop.h"
#include "macro.h"
#include "mon-util.h"
#include "notes.h"
#include "randart.h"
#include "skills2.h"
#include "stuff.h"
#include "view.h"
#include "items.h"

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

// buff must be at least ITEMNAME_SIZE if non-NULL. If NULL, a static
// item buffer will be used.
std::string item_def::name(description_level_type descrip,
                           bool terse, bool ident,
                           bool with_inscription,
                           bool quantity_words) const
{
    if (descrip == DESC_NONE)
        return ("");
    
    std::ostringstream buff;

    const std::string auxname = this->name_aux(descrip, terse, ident);
    const bool startvowel = is_vowel(auxname[0]);

    if (descrip == DESC_INVENTORY_EQUIP || descrip == DESC_INVENTORY) 
    {
        if (in_inventory(*this)) // actually in inventory
        {
            buff << index_to_letter(this->link);
            if ( terse)
                buff << ") ";
            else
                buff << " - ";
        }
        else 
            descrip = DESC_CAP_A;
    }

    if (terse)
        descrip = DESC_PLAIN;

    if (this->base_type == OBJ_ORBS
        || ( (ident || item_type_known( *this ))
             && ((this->base_type == OBJ_MISCELLANY
                  && this->sub_type == MISC_HORN_OF_GERYON)
                 || (is_fixed_artefact( *this )
                     || (is_random_artefact( *this ))))))
    {
        // artefacts always get "the" unless we just want the plain name
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
        case DESC_CAP_THE: buff << "The "; break;
        case DESC_NOCAP_THE: buff << "the "; break;
        case DESC_CAP_YOUR: buff << "Your "; break;
        case DESC_NOCAP_YOUR: buff << "your "; break;
        case DESC_NOCAP_ITS: buff << "its "; break;
        case DESC_CAP_A:
        case DESC_NOCAP_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
        case DESC_PLAIN:
        default:
            break;
        }

        if (descrip != DESC_BASENAME && descrip != DESC_QUALNAME)
        {
            if (quantity_words)
                buff << number_in_words(this->quantity) << " ";
            else
                buff << this->quantity << " ";
        }
    }
    else
    {
        switch (descrip)
        {
        case DESC_CAP_THE:   buff << "The "; break;
        case DESC_NOCAP_THE: buff << "the "; break;
        case DESC_CAP_A: buff << (startvowel ? "An " : "A "); break;

        case DESC_CAP_YOUR: buff << "Your "; break;
        case DESC_NOCAP_YOUR: buff << "your "; break;
        case DESC_NOCAP_ITS: buff << "its "; break;

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

    if (descrip == DESC_INVENTORY_EQUIP && this->x == -1 && this->y == -1)
    {
        ASSERT( this->link != -1 );

        if (this->link == you.equip[EQ_WEAPON])
        {
            if (you.inv[ you.equip[EQ_WEAPON] ].base_type == OBJ_WEAPONS
                || item_is_staff( you.inv[ you.equip[EQ_WEAPON] ] ))
            {
                buff << " (weapon)";
            }
            else
            {
                buff << " (in hand)";
            }
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
    }

    const bool  tried     = (!ident && item_type_tried(*this));
    std::string tried_str = "";

    if (tried)
    {
        item_type_id_state_type id_type =
            get_ident_type(this->base_type, this->sub_type);
        if (id_type == ID_MON_TRIED_TYPE)
            tried_str = "tried by monster";
        else
            tried_str = "tried";
    }

    if ( with_inscription && !(this->inscription.empty()) )
    {
        buff << " {";
        if ( tried )
            buff << tried_str << ", ";
        buff << this->inscription << "}";
    }
    else if ( tried )
        buff << " {" << tried_str << "}";

    return buff.str();
}


static const char* fixed_artefact_name( const item_def& item, bool ident )
{
    if (ident)
    {
        switch (item.special)
        {
        case SPWPN_SINGING_SWORD:       return "Singing Sword";
        case SPWPN_WRATH_OF_TROG:       return "Wrath of Trog";
        case SPWPN_SCYTHE_OF_CURSES:    return "Scythe of Curses";
        case SPWPN_MACE_OF_VARIABILITY: return "Mace of Variability";
        case SPWPN_GLAIVE_OF_PRUNE:     return "Glaive of Prune";
        case SPWPN_SCEPTRE_OF_TORMENT:  return "Sceptre of Torment";
        case SPWPN_SWORD_OF_ZONGULDROK: return "Sword of Zonguldrok";
        case SPWPN_SWORD_OF_CEREBOV:    return "Sword of Cerebov";
        case SPWPN_STAFF_OF_DISPATER:   return "Staff of Dispater";
        case SPWPN_SCEPTRE_OF_ASMODEUS: return "Sceptre of Asmodeus";
        case SPWPN_SWORD_OF_POWER:      return "Sword of Power";
        case SPWPN_KNIFE_OF_ACCURACY:   return "Knife of Accuracy";
        case SPWPN_STAFF_OF_OLGREB:     return "Staff of Olgreb";
        case SPWPN_VAMPIRES_TOOTH:      return "Vampire's Tooth";
        case SPWPN_STAFF_OF_WUCAD_MU:   return "Staff of Wucad Mu";
        default:                        return "Brodale's Buggy Bola";
        }
    }   
    else
    {
        switch (item.special)
        {
        case SPWPN_SINGING_SWORD:       return "golden long sword";
        case SPWPN_WRATH_OF_TROG:       return "bloodstained battleaxe";
        case SPWPN_SCYTHE_OF_CURSES:    return "warped scythe";
        case SPWPN_MACE_OF_VARIABILITY: return "shimmering mace";
        case SPWPN_GLAIVE_OF_PRUNE:     return "purple glaive";
        case SPWPN_SCEPTRE_OF_TORMENT:  return "jewelled golden mace";
        case SPWPN_SWORD_OF_ZONGULDROK: return "bone long sword";
        case SPWPN_SWORD_OF_CEREBOV:    return "great serpentine sword";
        case SPWPN_STAFF_OF_DISPATER:   return "golden staff";
        case SPWPN_SCEPTRE_OF_ASMODEUS: return "ruby sceptre";
        case SPWPN_SWORD_OF_POWER:      return "chunky great sword";
        case SPWPN_KNIFE_OF_ACCURACY:   return "thin dagger";
        case SPWPN_STAFF_OF_OLGREB:     return "green glowing staff";
        case SPWPN_VAMPIRES_TOOTH:      return "ivory dagger";
        case SPWPN_STAFF_OF_WUCAD_MU:   return "ephemeral quarterstaff";
        default:                        return "buggy bola";
        }
    }
}

static const char* weapon_brand_name(const item_def& item, bool terse)
{
    switch (get_weapon_brand(item))
    {
    case SPWPN_NORMAL: return "";
    case SPWPN_FLAMING: return ((terse) ? " (flame)" : " of flaming");
    case SPWPN_FREEZING: return ((terse) ? " (freeze)" : " of freezing");
    case SPWPN_HOLY_WRATH: return ((terse) ? " (holy)" : " of holy wrath");
    case SPWPN_ELECTROCUTION: return ((terse) ? " (elec)":" of electrocution");
    case SPWPN_ORC_SLAYING: return ((terse) ? " (slay orc)":" of orc slaying");
    case SPWPN_VENOM: return ((terse) ? " (venom)" : " of venom");
    case SPWPN_PROTECTION: return ((terse) ? " (protect)" : " of protection");
    case SPWPN_DRAINING: return ((terse) ? " (drain)" : " of draining");
    case SPWPN_SPEED: return ((terse) ? " (speed)" : " of speed");
    case SPWPN_DISRUPTION: return ((terse) ? " (disrupt)" : " of disruption");
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
        default:                      return "bugginess";
        }
    }
    else
    {
        switch (sparm)
        {
        case SPARM_RUNNING:           return " (run)";
        case SPARM_FIRE_RESISTANCE:   return " (R-fire)";
        case SPARM_COLD_RESISTANCE:   return " (R-cold)";
        case SPARM_POISON_RESISTANCE: return " (R-poison)";
        case SPARM_SEE_INVISIBLE:     return " (see invis)";
        case SPARM_DARKNESS:          return " (darkness)";
        case SPARM_STRENGTH:          return " (str)";
        case SPARM_DEXTERITY:         return " (dex)";
        case SPARM_INTELLIGENCE:      return " (int)";
        case SPARM_PONDEROUSNESS:     return " (ponderous)";
        case SPARM_LEVITATION:        return " (levitate)";
        case SPARM_MAGIC_RESISTANCE:  return " (R-magic)";
        case SPARM_PROTECTION:        return " (protect)";
        case SPARM_STEALTH:           return " (stealth)";
        case SPARM_RESISTANCE:        return " (resist)";
        case SPARM_POSITIVE_ENERGY:   return " (R-neg)"; // ha ha
        case SPARM_ARCHMAGI:          return " (Archmagi)";
        case SPARM_PRESERVATION:      return " (preserve)";
        default:                      return " (buggy)";
        }
    }
}

static const char* wand_type_name(int wandtype)
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
    case 11: return "plastic";
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
    case SCR_FORGETFULNESS:      return "forgetfulness";
    case SCR_ACQUIREMENT:        return "acquirement";
    case SCR_ENCHANT_WEAPON_II:  return "enchant weapon II";
    case SCR_VORPALISE_WEAPON:   return "vorpalise weapon";
    case SCR_RECHARGING:         return "recharging";
    case SCR_ENCHANT_WEAPON_III: return "enchant weapon III";
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
    case 13: return "plastic";
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
    case RUNE_SHOALS:      return "liquid";

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
        case MISC_DECK_OF_ESCAPE: return "deck of escape";
        case MISC_DECK_OF_DESTRUCTION: return "deck of destruction";
        case MISC_DECK_OF_DUNGEONS: return "deck of dungeons";
        case MISC_DECK_OF_SUMMONING: return "deck of summonings";
        case MISC_DECK_OF_WONDERS: return "deck of wonders";
        case MISC_DECK_OF_PUNISHMENT: return "deck of punishment";
        case MISC_DECK_OF_WAR: return "deck of war";
        case MISC_DECK_OF_CHANGES: return "deck of changes";
        case MISC_DECK_OF_DEFENSE: return "deck of defense";

        case MISC_CRYSTAL_BALL_OF_ENERGY: return "crystal ball of energy";
        case MISC_CRYSTAL_BALL_OF_FIXATION: return "crystal ball of fixation";
        case MISC_CRYSTAL_BALL_OF_SEEING: return "crystal ball of seeing";
        case MISC_BOX_OF_BEASTS: return "box of beasts";
        case MISC_EMPTY_EBONY_CASKET: return "empty ebony casket";
        case MISC_AIR_ELEMENTAL_FAN: return "air elemental fan";
        case MISC_LAMP_OF_FIRE: return "lamp of fire";
        case MISC_LANTERN_OF_SHADOWS: return "lantern of shadows";
        case MISC_HORN_OF_GERYON: return "horn of Geryon";
        case MISC_DISC_OF_STORMS: return "disc of storms";
        case MISC_STONE_OF_EARTH_ELEMENTALS:
            return "stone of earth elementals";
        case MISC_BOTTLED_EFREET: return "bottled efreet";

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
        case MISC_DECK_OF_DEFENSE:
            return "deck of cards";
        case MISC_CRYSTAL_BALL_OF_ENERGY:
        case MISC_CRYSTAL_BALL_OF_FIXATION:
        case MISC_CRYSTAL_BALL_OF_SEEING:
            return "crystal ball";
        case MISC_BOX_OF_BEASTS:
        case MISC_EMPTY_EBONY_CASKET:
            return "small ebony casket";
        case MISC_AIR_ELEMENTAL_FAN: return "gauzy fan";
        case MISC_LAMP_OF_FIRE: return "blazing lamp";
        case MISC_LANTERN_OF_SHADOWS: return "bone lantern";
        case MISC_HORN_OF_GERYON: return "silver horn";
        case MISC_DISC_OF_STORMS: return "grey disc";
        case MISC_STONE_OF_EARTH_ELEMENTALS: return "nondescript stone";
        case MISC_BOTTLED_EFREET: return "sealed bronze flask";

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
    case 0: return "paperback ";
    case 1: return "hardcover ";
    case 2: return "leatherbound ";
    case 3: return "metal-bound ";
    case 4: return "papyrus ";
    case 5: return "";
    case 6: return "";
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
    case BOOK_HEALING:                return "Healing";
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
    default:                          return "Bugginess";
    }
}

static const char* staff_primary_string(int p)
{
    switch (p)
    {
        
    case 0: return "curved";
    case 1: return "glowing";
    case 2: return "thick";
    case 3: return "thin";
    case 4: return "long";
    case 5: return "twisted";
    case 6: return "jewelled";
    case 7: return "runed";
    case 8: return "smoking";
    case 9: return "gnarled";
    default: return "buggy";
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

static const char* racial_description_string(const item_def& item, bool terse)
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
    if ( val >= 0 )
        os << '+';
    os << val;
}

// Note that "terse" is only currently used for the "in hand" listing on
// the game screen.
std::string item_def::name_aux( description_level_type desc,
                                bool terse, bool ident ) const
{
    // Shortcuts
    const int item_typ = this->sub_type;
    const int it_plus = this->plus;
    const int item_plus2 = this->plus2;

    const bool basename = desc == DESC_BASENAME;
    const bool qualname = desc == DESC_QUALNAME;
    
    const bool know_curse =
        !basename && !qualname
        && (ident || item_ident(*this, ISFLAG_KNOW_CURSE));
    
    const bool know_type = ident || item_type_known(*this);
    const bool know_pluses =
        !basename && !qualname
        && (ident || item_ident(*this, ISFLAG_KNOW_PLUSES));

    bool need_plural = true;
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
            if (item_cursed( *this ))
                buff << "cursed ";
            else if (Options.show_uncursed && !know_pluses)
                buff << "uncursed ";
        }

        if (know_pluses)
        {
            if ( terse && (it_plus == item_plus2) )
                output_with_sign(buff, it_plus);
            else
            {
                output_with_sign(buff, it_plus);
                buff << ',';
                output_with_sign(buff, item_plus2);
            }
            buff << " ";
        }

        if (is_random_artefact( *this ))
        {
            buff << randart_name(*this);
            break;
        }

        if (is_fixed_artefact( *this ))
        {
            buff << fixed_artefact_name( *this, know_type );
            break;
        }

        // Now that we can have "glowing elven" weapons, it's 
        // probably a good idea to cut out the descriptive
        // term once it's become obsolete. -- bwr
        if (!know_pluses && !terse && !basename)
        {
            switch (get_equip_desc( *this ))
            {
            case ISFLAG_RUNED:
                buff << "runed ";
                break;
            case ISFLAG_GLOWING:
                buff << "glowing ";
                break;
            }
        } 

        if (!basename)
        {
            // always give racial type (it does have game effects)
            buff << racial_description_string(*this, terse);

            if (know_type && !terse &&
                (get_weapon_brand(*this) == SPWPN_VAMPIRICISM))
                buff << "vampiric ";
        }
        buff << item_base_name(*this);
            
        if (!basename)
        {
            if ( know_type )
                buff << weapon_brand_name(*this, terse);

            if (know_curse && item_cursed(*this) && terse)
                buff << " (curse)";
        }
        break;

    case OBJ_MISSILES:
        brand = get_ammo_brand( *this );

        if (!basename)
        {
            if (brand == SPMSL_POISONED)
                buff << ((terse) ? "poison " : "poisoned ");
        
            if (brand == SPMSL_CURARE)
                buff << ((terse) ? "curare " : "curare-tipped ");
        }

        if (know_pluses)
        {
            output_with_sign(buff, it_plus);
            buff << ' ';
        }

        if (!basename)
            buff << racial_description_string(*this, terse);

        buff << ammo_name(static_cast<missile_type>(item_typ));

        if (know_type && !basename)
        {
            switch (brand)
            {
            case SPMSL_FLAME:
                buff << ((terse) ? " (flame)" : " of flame");
                break;
            case SPMSL_ICE:
                buff << ((terse) ? " (ice)" : " of ice");
                break;
            case SPMSL_NORMAL:
            case SPMSL_POISONED:
            case SPMSL_CURARE:
                break;
            default:
                buff << " (buggy)";
            }
        }
        break;

    case OBJ_ARMOUR:
        if (know_curse && !terse)
        {
            if (item_cursed( *this ))
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
        if (is_random_artefact( *this ) && !basename)
        {
            buff << randart_armour_name(*this);
            break;
        }

        // Now that we can have "glowing elven" armour, it's 
        // probably a good idea to cut out the descriptive
        // term once it's become obsolete. -- bwr
        if (!know_pluses && !terse & !basename)
        {
            switch (get_equip_desc( *this ))
            {
            case ISFLAG_EMBROIDERED_SHINY:
                if (item_typ == ARM_ROBE || item_typ == ARM_CLOAK
                    || item_typ == ARM_GLOVES || item_typ == ARM_BOOTS)
                {
                    buff << "embroidered ";
                }
                else if (item_typ != ARM_LEATHER_ARMOUR 
                        && item_typ != ARM_ANIMAL_SKIN)
                {
                    buff << "shiny ";
                }
                break;

            case ISFLAG_RUNED:
                buff << "runed ";
                break;

            case ISFLAG_GLOWING:
                buff << "glowing ";
                break;
            }
        }

        if (!basename)
        {
            // always give racial description (has game effects)
            buff << racial_description_string(*this, terse);
        }

        buff << item_base_name(*this);

        if (!basename)
        {
            const special_armour_type sparm = get_armour_ego_type( *this );

            if (know_type && sparm != SPARM_NORMAL)
            {
                if ( !terse )
                    buff << " of ";
                buff << armour_ego_name(sparm, terse);
            }
        }

        if (know_curse && item_cursed(*this) && terse && !basename)
            buff << " (curse)";
        break;

    case OBJ_WANDS:
        if (basename)
        {
            buff << "wand";
            break;
        }
        if (know_type)
        {
            buff << "wand of " << wand_type_name(item_typ);
        }
        else
        {
            buff << wand_secondary_string(this->special / 12)
                 << wand_primary_string(this->special % 12)
                 << " wand";
        }

        if (know_pluses)
            buff << " (" << it_plus << ")";
        else if (item_plus2 == ZAPCOUNT_EMPTY)
            buff << " {empty}";
        else if (item_plus2 > 0)
            buff << " {zapped: " << item_plus2
                 << '}';
        break;

    case OBJ_POTIONS:
        if (basename)
        {
            buff << "potion";
            break;
        }
        if (know_type)
        {
            buff << "potion of " << potion_type_name(item_typ);
        }
        else
        {
            const int pqual   = PQUAL(this->special);
            const int pcolour = PCOLOUR(this->special);

            static const char *potion_qualifiers[] = {
                "",  "bubbling ", "fuming ", "fizzy ", "viscous ", "lumpy ",
                "smoky ", "glowing ", "sedimented ", "metallic ", "murky ",
                "gluggy ", "oily ", "slimy ", "emulsified "
            };
            ASSERT( sizeof(potion_qualifiers) / sizeof(*potion_qualifiers)
                        == PDQ_NQUALS );

            static const char *potion_colours[] = {
                "clear", "blue", "black", "silvery", "cyan", "purple",
                "orange", "inky", "red", "yellow", "green", "brown", "pink",
                "white"
            };
            ASSERT( sizeof(potion_colours) / sizeof(*potion_colours)
                        == PDC_NCOLOURS );

            const char *qualifier = 
                (pqual < 0 || pqual >= PDQ_NQUALS)? "bug-filled "
                                    : potion_qualifiers[pqual];

            const char *clr =
                (pcolour < 0 || pcolour >= PDC_NCOLOURS)? "bogus"
                                    : potion_colours[pcolour];

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
            if (this->special < 100)
                buff << "rotting ";

            buff << "chunk of "
                 << mons_type_name(it_plus, DESC_PLAIN)
                 << " flesh";
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
        if (basename)
        {
            if ( jewellery_is_amulet(*this) )
                buff << "amulet";
            else                // i.e., an amulet
                buff << "ring";

            break;
        }

        // not using {tried} here because there are some confusing 
        // issues to work out with how we want to handle jewellery 
        // artefacts and base type id. -- bwr
        if (know_curse)
        {
            if (item_cursed( *this ))
                buff << "cursed ";
            else if (Options.show_uncursed && !terse
                     && (!ring_has_pluses(*this) || !know_pluses)
                     // If the item is worn, its curse status is known,
                     // no need to belabour the obvious.
                     && get_equip_slot( this ) == -1)
            {
                buff << "uncursed ";
            }
        }

        if (is_random_artefact( *this ))
        {
            buff << randart_jewellery_name(*this);
            break;
        }

        if (know_type)
        {
            if (know_pluses && ring_has_pluses(*this) && !basename && !qualname)
            {
                output_with_sign(buff, it_plus);

                if ( ring_has_pluses(*this) == 2 )
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
            if ( jewellery_is_amulet(*this) )
            {
                buff << amulet_secondary_string(this->special / 13)
                     << amulet_primary_string(this->special % 13)
                     << " amulet";
            }
            else                // i.e., an amulet
            {
                buff << ring_secondary_string(this->special / 13)
                     << ring_primary_string(this->special % 13)
                     << " ring";
            }
        }
        break;

    case OBJ_MISCELLANY:
        if ( item_typ == MISC_RUNE_OF_ZOT )
        {
            buff << rune_type_name(it_plus) << " rune";

            if (know_type)
                buff << " of Zot";
        }
        else
        {
            if ( is_deck(*this) )
            {
                if (basename)
                {
                    buff << "deck of cards";
                    break;
                }
                buff << deck_rarity_name(deck_rarity(*this)) << ' ';
            }
            buff << misc_type_name(item_typ, know_type);
            if ( is_deck(*this) && item_plus2 != 0 )
            {
                // an inscribed deck!
                buff << " {"
                     << card_name(static_cast<card_type>(item_plus2 - 1))
                     << "}";
            }
        }
        break;

    case OBJ_BOOKS:
        if (!know_type)
        {
            buff << book_secondary_string(this->special / 10)
                 << book_primary_string(this->special % 10)
                 << "book";
        }
        else if (item_typ == BOOK_MANUAL)
            buff << "manual of " << skill_name(it_plus);
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
                buff << staff_primary_string(this->special) << " ";
                
            buff << (item_is_rod( *this ) ? "rod" : "staff");
        }      
        else
        {
            buff << (item_is_rod( *this ) ? "rod" : "staff")
                 << " of " << staff_type_name(item_typ);

            if (item_is_rod(*this) && !basename)
            {
                buff << " (" << (this->plus / ROD_CHARGE_MULT)
                     << "/" << (this->plus2 / ROD_CHARGE_MULT)
                     << ")";
            }
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
        if (item_typ == CORPSE_BODY && this->special < 100)
        {
            buff << "rotting ";
        }
        {
            buff << mons_type_name(it_plus, DESC_PLAIN) << ' ';
            if (item_typ == CORPSE_BODY)
                buff << "corpse";
            else if (item_typ == CORPSE_SKELETON)
                buff << "skeleton";
            else
                buff << "corpse bug";
        }
        break;

    default:
        buff << "!";
    }

    // One plural to rule them all.
    if (need_plural && this->quantity > 1 && !basename && !qualname)
        buff.str( pluralise(buff.str()) );

    // Disambiguation
    if (!terse && !basename && know_type)
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
                buff << " [iron,fireball,lightning]";
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
        default:
            break;
        }
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
    default:            return (NUM_IDTYPE);
    }
}

bool item_type_known( const item_def& item )
{
    if (item_ident(item, ISFLAG_KNOW_TYPE))
        return (true);

    // Artefacts have different descriptions from other items,
    // so we can't use general item knowledge for them.
    if ( is_artefact(item) )
        return false;

    const item_type_id_type idt = objtype_to_idtype(item.base_type);
    if ( idt != NUM_IDTYPE && item.sub_type < 50  )
        return ( type_ids[idt][item.sub_type] == ID_KNOWN_TYPE );
    else
        return false;
}

bool item_type_tried( const item_def& item )
{
    if ( item_type_known(item) )
        return false;

    // Well, if we ever put in scroll or potion artefacts, this
    // might be necessary...
    if ( is_artefact(item) )
        return false;

    const item_type_id_type idt = objtype_to_idtype(item.base_type);
    if (idt != NUM_IDTYPE && item.sub_type < 50)
        return ( type_ids[idt][item.sub_type] == ID_TRIED_TYPE
                 || type_ids[idt][item.sub_type] == ID_MON_TRIED_TYPE);
    else
        return false;
}

id_arr& get_typeid_array()
{
    return type_ids;
}

void set_ident_type( object_class_type basetype, int subtype,
                     item_type_id_state_type setting, bool force )
{
    // Don't allow overwriting of known type with tried unless forced.
    if (!force 
        && (setting == ID_MON_TRIED_TYPE || setting == ID_TRIED_TYPE)
        && setting <= get_ident_type( basetype, subtype ))
    {
        return;
    }

    const item_type_id_type idt = objtype_to_idtype(basetype);

    if ( idt != NUM_IDTYPE )
    {
        if (type_ids[idt][subtype] != setting)
        {
            type_ids[idt][subtype] = setting;
            request_autoinscribe();
        }
    }
}

item_type_id_state_type get_ident_type(object_class_type basetype, int subtype)
{
    const item_type_id_type idt = objtype_to_idtype(basetype);
    if ( idt != NUM_IDTYPE && subtype < type_ids.height() )
        return type_ids[idt][subtype];
    else
        return ID_UNKNOWN_TYPE;
}

static MenuEntry *discoveries_item_mangle(MenuEntry *me)
{
    InvEntry *ie = dynamic_cast<InvEntry*>(me);
    MenuEntry *newme = new MenuEntry;
    newme->text = std::string(" ") + ie->item->name(DESC_PLAIN);
    newme->quantity = 0;
    delete me;

    return (newme);
}

bool item_names( const item_def *it1,
                 const item_def *it2 )
{
    return it1->name(DESC_PLAIN, false, false, false)
           < it2->name(DESC_PLAIN, false, false, false);
}

void check_item_knowledge()
{
    std::vector<const item_def*> items;

    const object_class_type idx_to_objtype[4] = { OBJ_WANDS, OBJ_SCROLLS,
                                                  OBJ_JEWELLERY, OBJ_POTIONS };
    const int idx_to_maxtype[4] = { NUM_WANDS, NUM_SCROLLS,
                                    NUM_JEWELLERY, NUM_POTIONS };

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < idx_to_maxtype[i]; j++)
        {
            if (type_ids[i][j] == ID_KNOWN_TYPE)
            {
                item_def* ptmp = new item_def;
                if ( ptmp != 0 )
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
        mpr("You don't recognise anything yet!");
    else
    {
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
}                               // end check_item_knowledge()


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
        && (is_random_artefact(item) ||
            is_unrandom_artefact(item) ||
            is_fixed_artefact(item)))
        return (true);

    const std::string iname = item.name(DESC_PLAIN);
    for (unsigned i = 0; i < Options.note_items.size(); ++i)
        if (Options.note_items[i].matches(iname))
            return (true);
    return (false);
}

const std::string menu_colour_item_prefix(const item_def &item)
{
    std::string str = "";

    if (Options.menu_colour_prefix_id)
    {
        if (item_ident(item, ISFLAG_KNOW_TYPE)
            || item.base_type == OBJ_FOOD
            || item.base_type == OBJ_CORPSES)
        {
            str += "identified ";
        }
        else
        {
            if (get_ident_type(item.base_type,
                               item.sub_type) == ID_KNOWN_TYPE)
            {
                // Wands are only fully identified if we know the
                // number of charges.
                if (item.base_type == OBJ_WANDS)
                    str += "known ";
                // Rings are fully identified simply by knowing their
                // type, unless the ring has plusses, like a ring of
                // dexterity.
                else if (item.base_type == OBJ_JEWELLERY
                         && !jewellery_is_amulet(item))
                {
                    if (item.plus == 0 && item.plus2 == 0)
                        str += "identified ";
                    else
                        str += "known ";
                }
                // All other types of magical items are fully identified
                // simply by knowing the type
                else
                    str += "identified ";
            }
            else
                str += "unidentified ";
        }
    }

    if (Options.menu_colour_prefix_class)
    {
        str += item_class_name(item.base_type, true) + " ";
    }

    return str;
}


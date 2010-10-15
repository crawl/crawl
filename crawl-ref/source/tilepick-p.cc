#include "AppHdr.h"

#ifdef USE_TILE
#include "tilepick-p.h"

#include <stdio.h>

#include "artefact.h"
#include "describe.h"
#include "itemname.h"
#include "itemprop.h"
#include "player.h"
#include "tiledef-player.h"
#include "tiledef-unrand.h"
#include "tiledoll.h"
#include "tilepick.h"
#include "transform.h"

static tileidx_t _modrng(int mod, tileidx_t first, tileidx_t last)
{
    return (first + mod % (last - first + 1));
}

tileidx_t tilep_equ_weapon(const item_def &item)
{
    if (item.base_type == OBJ_STAVES)
    {
        // Can't just use item.special here as STAFF_POWER abuses
        // item.special for storing the MP in case of quick re-wield.
        int orig_special = you.item_description[IDESC_STAVES][item.sub_type];
        int desc = (orig_special / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;
        if (item_is_rod(item))
            return TILEP_HAND1_ROD_BROWN + desc;
        else
            return TILEP_HAND1_STAFF_LARGE + desc;
    }

    if (item.base_type == OBJ_MISCELLANY)
    {
        switch (item.sub_type)
        {
        case MISC_BOTTLED_EFREET:             return TILEP_HAND1_BOTTLE;
        case MISC_AIR_ELEMENTAL_FAN:          return TILEP_HAND1_FAN;
        case MISC_STONE_OF_EARTH_ELEMENTALS:  return TILEP_HAND1_STONE;
        case MISC_DISC_OF_STORMS:             return TILEP_HAND1_DISC;

        case MISC_CRYSTAL_BALL_OF_SEEING:
        case MISC_CRYSTAL_BALL_OF_ENERGY:
        case MISC_CRYSTAL_BALL_OF_FIXATION:   return TILEP_HAND1_CRYSTAL;

        case MISC_LAMP_OF_FIRE:               return TILEP_HAND1_LANTERN;
        case MISC_LANTERN_OF_SHADOWS:         return TILEP_HAND1_BONE_LANTERN;
        case MISC_HORN_OF_GERYON:             return TILEP_HAND1_HORN;

        case MISC_BOX_OF_BEASTS:
        case MISC_EMPTY_EBONY_CASKET:         return TILEP_HAND1_BOX;

        case MISC_DECK_OF_ESCAPE:
        case MISC_DECK_OF_DESTRUCTION:
        case MISC_DECK_OF_DUNGEONS:
        case MISC_DECK_OF_SUMMONING:
        case MISC_DECK_OF_WONDERS:
        case MISC_DECK_OF_PUNISHMENT:
        case MISC_DECK_OF_WAR:
        case MISC_DECK_OF_CHANGES:
        case MISC_DECK_OF_DEFENCE:            return TILEP_HAND1_DECK;
        }
    }

    if (item.base_type != OBJ_WEAPONS)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    switch (item.sub_type)
    {
    // Blunt
    case WPN_CLUB:              return TILEP_HAND1_CLUB_SLANT;
    case WPN_MACE:              return TILEP_HAND1_MACE;
    case WPN_GREAT_MACE:        return TILEP_HAND1_GREAT_MACE;
    case WPN_FLAIL:             return TILEP_HAND1_FLAIL;
    case WPN_SPIKED_FLAIL:      return TILEP_HAND1_SPIKED_FLAIL;
    case WPN_DIRE_FLAIL:        return TILEP_HAND1_GREAT_FLAIL;
    case WPN_MORNINGSTAR:       return TILEP_HAND1_MORNINGSTAR;
    case WPN_EVENINGSTAR:       return TILEP_HAND1_EVENINGSTAR;
    case WPN_GIANT_CLUB:        return TILEP_HAND1_GIANT_CLUB_PLAIN;
    case WPN_GIANT_SPIKED_CLUB: return TILEP_HAND1_GIANT_CLUB_SPIKE_SLANT;
    case WPN_ANKUS:             return TILEP_HAND1_MACE;
    case WPN_WHIP:              return TILEP_HAND1_WHIP;
    case WPN_DEMON_WHIP:        return TILEP_HAND1_BLACK_WHIP;
    case WPN_SACRED_SCOURGE:    return TILEP_HAND1_SACRED_SCOURGE;

    // Edge
    case WPN_KNIFE:                return TILEP_HAND1_DAGGER_SLANT;
    case WPN_DAGGER:               return TILEP_HAND1_DAGGER_SLANT;
    case WPN_SHORT_SWORD:          return TILEP_HAND1_SHORT_SWORD_SLANT;
    case WPN_LONG_SWORD:           return TILEP_HAND1_LONG_SWORD_SLANT;
    case WPN_GREAT_SWORD:          return TILEP_HAND1_GREAT_SWORD_SLANT;
    case WPN_SCIMITAR:             return TILEP_HAND1_SCIMITAR;
    case WPN_FALCHION:             return TILEP_HAND1_FALCHION;
    case WPN_SABRE:                return TILEP_HAND1_SABRE;
    case WPN_DEMON_BLADE:          return TILEP_HAND1_DEMON_BLADE;
    case WPN_QUICK_BLADE:          return TILEP_HAND1_DAGGER;
    case WPN_KATANA:               return TILEP_HAND1_KATANA_SLANT;
    case WPN_DOUBLE_SWORD:         return TILEP_HAND1_DOUBLE_SWORD;
    case WPN_TRIPLE_SWORD:         return TILEP_HAND1_TRIPLE_SWORD;
    case WPN_EUDEMON_BLADE:        return TILEP_HAND1_BLESSED_BLADE;
    // new blessed weapons
    case WPN_BLESSED_LONG_SWORD:   return TILEP_HAND1_LONG_SWORD_SLANT;
    case WPN_BLESSED_GREAT_SWORD:  return TILEP_HAND1_GREAT_SWORD_SLANT;
    case WPN_BLESSED_SCIMITAR:     return TILEP_HAND1_SCIMITAR;
    case WPN_BLESSED_FALCHION:     return TILEP_HAND1_FALCHION;
    case WPN_BLESSED_KATANA:       return TILEP_HAND1_KATANA_SLANT;
    case WPN_BLESSED_DOUBLE_SWORD: return TILEP_HAND1_DOUBLE_SWORD;
    case WPN_BLESSED_TRIPLE_SWORD: return TILEP_HAND1_TRIPLE_SWORD;

    // Axe
    case WPN_HAND_AXE:         return TILEP_HAND1_HAND_AXE;
    case WPN_BATTLEAXE:        return TILEP_HAND1_BATTLEAXE;
    case WPN_BROAD_AXE:        return TILEP_HAND1_BROAD_AXE;
    case WPN_WAR_AXE:          return TILEP_HAND1_WAR_AXE;
    case WPN_EXECUTIONERS_AXE: return TILEP_HAND1_EXECUTIONERS_AXE;
    case WPN_BARDICHE:         return TILEP_HAND1_GLAIVE3;

    // Pole
    case WPN_SPEAR:         return TILEP_HAND1_SPEAR;
    case WPN_HALBERD:       return TILEP_HAND1_HALBERD;
    case WPN_GLAIVE:        return TILEP_HAND1_GLAIVE;
    case WPN_QUARTERSTAFF:  return TILEP_HAND1_QUARTERSTAFF1;
    case WPN_LAJATANG:      return TILEP_HAND1_DIRE_LAJATANG;
    case WPN_SCYTHE:        return TILEP_HAND1_SCYTHE;
    case WPN_HAMMER:        return TILEP_HAND1_HAMMER;
    case WPN_TRIDENT:       return TILEP_HAND1_TRIDENT2;
    case WPN_DEMON_TRIDENT: return TILEP_HAND1_DEMON_TRIDENT;
    case WPN_TRISHULA:      return TILEP_HAND1_TRISHULA;

    // Ranged
    case WPN_SLING:         return TILEP_HAND1_SLING;
    case WPN_BOW:           return TILEP_HAND1_BOW2;
    case WPN_CROSSBOW:      return TILEP_HAND1_CROSSBOW;
    case WPN_BLOWGUN:       return TILEP_HAND1_BLOWGUN;
    case WPN_LONGBOW:       return TILEP_HAND1_BOW3;

    default: return 0;
    }
}

tileidx_t tilep_equ_shield(const item_def &item)
{
    if (you.equip[EQ_SHIELD] == -1)
        return 0;

    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    switch (item.sub_type)
    {
        case ARM_SHIELD:       return TILEP_HAND2_SHIELD_KNIGHT_BLUE;
        case ARM_BUCKLER:      return TILEP_HAND2_SHIELD_ROUND_SMALL;
        case ARM_LARGE_SHIELD: return TILEP_HAND2_SHIELD_LONG_RED;
        default: return 0;
    }
}

tileidx_t tilep_equ_armour(const item_def &item)
{
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    switch (item.sub_type)
    {

    case ARM_ROBE:
        return _modrng(item.rnd, TILEP_BODY_ROBE_FIRST_NORM,
                       TILEP_BODY_ROBE_LAST_NORM);

    case ARM_LEATHER_ARMOUR:     return TILEP_BODY_LEATHER_ARMOUR3;
    case ARM_RING_MAIL:          return TILEP_BODY_RINGMAIL;
    case ARM_CHAIN_MAIL:         return TILEP_BODY_CHAINMAIL;
    case ARM_SCALE_MAIL:         return TILEP_BODY_SCALEMAIL;
    case ARM_SPLINT_MAIL:        return TILEP_BODY_BANDED;
    case ARM_BANDED_MAIL:        return TILEP_BODY_BANDED;
    case ARM_PLATE_MAIL:         return TILEP_BODY_PLATE_BLACK;
    case ARM_CRYSTAL_PLATE_MAIL: return TILEP_BODY_CRYSTAL_PLATE;

    case ARM_DRAGON_HIDE:         return TILEP_BODY_DRAGONSC_GREEN;
    case ARM_ICE_DRAGON_HIDE:     return TILEP_BODY_DRAGONSC_CYAN;
    case ARM_STEAM_DRAGON_HIDE:   return TILEP_BODY_DRAGONSC_WHITE;
    case ARM_MOTTLED_DRAGON_HIDE: return TILEP_BODY_DRAGONSC_MAGENTA;
    case ARM_STORM_DRAGON_HIDE:   return TILEP_BODY_DRAGONSC_BLUE;
    case ARM_GOLD_DRAGON_HIDE:    return TILEP_BODY_DRAGONSC_GOLD;
    case ARM_SWAMP_DRAGON_HIDE:   return TILEP_BODY_DRAGONSC_BROWN;

    case ARM_DRAGON_ARMOUR:         return TILEP_BODY_DRAGONARM_GREEN;
    case ARM_ICE_DRAGON_ARMOUR:     return TILEP_BODY_DRAGONARM_CYAN;
    case ARM_STEAM_DRAGON_ARMOUR:   return TILEP_BODY_DRAGONARM_WHITE;
    case ARM_MOTTLED_DRAGON_ARMOUR: return TILEP_BODY_DRAGONARM_MAGENTA;
    case ARM_STORM_DRAGON_ARMOUR:   return TILEP_BODY_DRAGONARM_BLUE;
    case ARM_GOLD_DRAGON_ARMOUR:    return TILEP_BODY_DRAGONARM_GOLD;
    case ARM_SWAMP_DRAGON_ARMOUR:   return TILEP_BODY_DRAGONARM_BROWN;

    case ARM_ANIMAL_SKIN:          return TILEP_BODY_ANIMAL_SKIN;
    case ARM_TROLL_HIDE:
    case ARM_TROLL_LEATHER_ARMOUR: return TILEP_BODY_TROLL_HIDE;

    default: return 0;
    }
}

tileidx_t tilep_equ_cloak(const item_def &item)
{
    if (you.equip[EQ_CLOAK] == -1)
        return 0;

    if (item.base_type != OBJ_ARMOUR || item.sub_type != ARM_CLOAK)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    return _modrng(item.rnd, TILEP_CLOAK_FIRST_NORM, TILEP_CLOAK_LAST_NORM);
}

tileidx_t tilep_equ_helm(const item_def &item)
{
    if (you.equip[EQ_HELMET] == -1)
        return 0;
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;

        // Although there shouldn't be any, just in case
        // unhandled artefacts fall through to defaults...
    }

    int etype = enchant_to_int(item);
    int helmet_desc = get_helmet_desc(item);
    switch (item.sub_type)
    {
        case ARM_CAP:
            return _modrng(item.rnd, TILEP_HELM_CAP_FIRST_NORM,
                           TILEP_HELM_CAP_LAST_NORM);

        case ARM_WIZARD_HAT:
            return _modrng(item.rnd, TILEP_HELM_WHAT_FIRST_NORM,
                           TILEP_HELM_WHAT_LAST_NORM);

        case ARM_HELMET:
            switch (helmet_desc)
            {
                case THELM_DESC_PLAIN:
                    switch (etype)
                    {
                        default:
                            return TILEP_HELM_CHAIN;
                        case 1:
                            return TILEP_HELM_HELM_GIMLI;
                        case 2:
                            return TILEP_HELM_HELM_IRON;
                        case 3:
                            return TILEP_HELM_FHELM_GRAY2;
                        case 4:
                            return TILEP_HELM_FHELM_BLACK;
                    }
                case THELM_DESC_WINGED:
                    return TILEP_HELM_YELLOW_WING;
                case THELM_DESC_HORNED:
                    switch (etype)
                    {
                        default:
                            return TILEP_HELM_FHELM_HORN2;
                        case 1:
                            return TILEP_HELM_BLUE_HORN_GOLD;
                        case 2:
                            return TILEP_HELM_FHELM_HORN_GRAY;
                        case 3:
                            return TILEP_HELM_FHELM_HORN_YELLOW;
                        case 4:
                            return TILEP_HELM_FHELM_HORN_BLACK;
                    }
                case THELM_DESC_CRESTED:
                    return TILEP_HELM_FHELM_ISILDUR;
                case THELM_DESC_PLUMED:
                    return TILEP_HELM_FHELM_PLUME;
                case THELM_DESC_SPIKED:
                    return TILEP_HELM_FHELM_EVIL;
                case THELM_DESC_VISORED:
                    return TILEP_HELM_FHELM_GRAY3;
                case THELM_DESC_GOLDEN:
                    return TILEP_HELM_FULL_GOLD;
            }
    }

    return 0;
}

tileidx_t tilep_equ_gloves(const item_def &item)
{
    if (you.equip[EQ_GLOVES] == -1)
        return 0;
    if (item.base_type != OBJ_ARMOUR || item.sub_type != ARM_GLOVES)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    return _modrng(item.rnd, TILEP_ARM_FIRST_NORM, TILEP_ARM_LAST_NORM);
}

tileidx_t tilep_equ_boots(const item_def &item)
{
    if (you.equip[EQ_BOOTS] == -1)
        return 0;
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    int etype = enchant_to_int(item);

    if (item.sub_type == ARM_NAGA_BARDING)
        return TILEP_BOOTS_NAGA_BARDING + std::min(etype, 3);

    if (item.sub_type == ARM_CENTAUR_BARDING)
        return TILEP_BOOTS_CENTAUR_BARDING + std::min(etype, 3);

    if (item.sub_type != ARM_BOOTS)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    return _modrng(item.rnd, TILEP_BOOTS_FIRST_NORM, TILEP_BOOTS_LAST_NORM);
}

tileidx_t tileidx_player()
{
    int ch = TILEP_PLAYER;

    // Handle shapechange first
    switch (you.attribute[ATTR_TRANSFORMATION])
    {
        // animals
        case TRAN_BAT:       ch = TILEP_TRAN_BAT;       break;
        case TRAN_SPIDER:    ch = TILEP_TRAN_SPIDER;    break;
        case TRAN_PIG:       ch = TILEP_TRAN_PIG;       break;
        // non-animals
        case TRAN_ICE_BEAST: ch = TILEP_TRAN_ICE_BEAST; break;
        case TRAN_STATUE:    ch = TILEP_TRAN_STATUE;    break;
        case TRAN_DRAGON:    ch = TILEP_TRAN_DRAGON;    break;
        case TRAN_LICH:      ch = TILEP_TRAN_LICH;      break;
    }

    if (you.airborne())
        ch |= TILE_FLAG_FLYING;

    if (you.attribute[ATTR_HELD])
        ch |= TILE_FLAG_NET;

    return ch;
}

bool is_player_tile(tileidx_t tile, tileidx_t base_tile)
{
    return (tile >= base_tile
            && tile < base_tile + tile_player_count(base_tile));
}

static int _draconian_colour(int race, int level)
{
    if (level < 0) // hack:monster
    {
        switch (race)
        {
        case MONS_DRACONIAN:        return (0);
        case MONS_BLACK_DRACONIAN:  return (1);
        case MONS_YELLOW_DRACONIAN: return (2);
        case MONS_GREY_DRACONIAN:   return (3);
        case MONS_GREEN_DRACONIAN:  return (4);
        case MONS_MOTTLED_DRACONIAN:return (5);
        case MONS_PALE_DRACONIAN:   return (6);
        case MONS_PURPLE_DRACONIAN: return (7);
        case MONS_RED_DRACONIAN:    return (8);
        case MONS_WHITE_DRACONIAN:  return (9);
        }
    }
    switch (race)
    {
    case SP_BLACK_DRACONIAN:   return (1);
    case SP_YELLOW_DRACONIAN:  return (2);
    case SP_GREY_DRACONIAN:    return (3);
    case SP_GREEN_DRACONIAN:   return (4);
    case SP_MOTTLED_DRACONIAN: return (5);
    case SP_PALE_DRACONIAN:    return (6);
    case SP_PURPLE_DRACONIAN:  return (7);
    case SP_RED_DRACONIAN:     return (8);
    case SP_WHITE_DRACONIAN:   return (9);
    }
    return (0);
}

tileidx_t tilep_species_to_base_tile(int sp, int level)
{
    switch (sp)
    {
    case SP_HUMAN:
        return TILEP_BASE_HUMAN;
    case SP_HIGH_ELF:
    case SP_SLUDGE_ELF:
        return TILEP_BASE_ELF;
    case SP_DEEP_ELF:
        return TILEP_BASE_DEEP_ELF;
    case SP_MOUNTAIN_DWARF:
        return TILEP_BASE_DWARF;
    case SP_HALFLING:
        return TILEP_BASE_HALFLING;
    case SP_HILL_ORC:
        return TILEP_BASE_ORC;
    case SP_KOBOLD:
        return TILEP_BASE_KOBOLD;
    case SP_MUMMY:
        return TILEP_BASE_MUMMY;
    case SP_NAGA:
        return TILEP_BASE_NAGA;
    case SP_OGRE:
        return TILEP_BASE_OGRE;
    case SP_TROLL:
        return TILEP_BASE_TROLL;
    case SP_BASE_DRACONIAN:
    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    {
        const int colour_offset = _draconian_colour(sp, level);
        return (TILEP_BASE_DRACONIAN + colour_offset * 2);
    }
    case SP_CENTAUR:
        return TILEP_BASE_CENTAUR;
    case SP_DEMIGOD:
        return TILEP_BASE_DEMIGOD;
    case SP_SPRIGGAN:
        return TILEP_BASE_SPRIGGAN;
    case SP_MINOTAUR:
        return TILEP_BASE_MINOTAUR;
    case SP_DEMONSPAWN:
        return TILEP_BASE_DEMONSPAWN;
    case SP_GHOUL:
        return TILEP_BASE_GHOUL;
    case SP_KENKU:
        return TILEP_BASE_KENKU;
    case SP_MERFOLK:
        return TILEP_BASE_MERFOLK;
    case SP_VAMPIRE:
        return TILEP_BASE_VAMPIRE;
    case SP_DEEP_DWARF:
        return TILEP_BASE_DEEP_DWARF;
    case SP_CAT:
        return TILEP_BASE_FELID;
    default:
        return TILEP_BASE_HUMAN;
    }
}
void tilep_draconian_init(int sp, int level, tileidx_t *base,
                          tileidx_t *head, tileidx_t *wing)
{
    const int colour_offset = _draconian_colour(sp, level);
    *base = TILEP_BASE_DRACONIAN + colour_offset * 2;
    *head = tile_player_part_start[TILEP_PART_DRCHEAD] + colour_offset;

    if (player_mutation_level(MUT_BIG_WINGS))
        *wing = tile_player_part_start[TILEP_PART_DRCWING] + colour_offset;
}

// Set default parts of each race: body + optional beard, hair, etc.
// This function needs to be entirely deterministic.
void tilep_race_default(int sp, int level, dolls_data *doll)
{
    tileidx_t *parts = doll->parts;

    tileidx_t result = tilep_species_to_base_tile(sp, level);
    if (parts[TILEP_PART_BASE] != TILEP_SHOW_EQUIP)
        result = parts[TILEP_PART_BASE];

    tileidx_t hair   = 0;
    tileidx_t beard  = 0;
    tileidx_t wing   = 0;
    tileidx_t head   = 0;

    hair = TILEP_HAIR_SHORT_BLACK;

    switch (sp)
    {
        case SP_ELF:
        case SP_HIGH_ELF:
        case SP_SLUDGE_ELF:
            hair = TILEP_HAIR_ELF_YELLOW;
            break;
        case SP_DEEP_ELF:
            hair = TILEP_HAIR_ELF_WHITE;
            break;
        case SP_HILL_DWARF:
        case SP_MOUNTAIN_DWARF:
            hair  = TILEP_HAIR_LONG_RED;
            beard = TILEP_BEARD_LONG_RED;
            break;
        case SP_HILL_ORC:
            hair = 0;
            break;
        case SP_KOBOLD:
            hair = 0;
            break;
        case SP_MUMMY:
            hair = 0;
            break;
        case SP_TROLL:
            hair = 0;
            break;
        case SP_BASE_DRACONIAN:
        case SP_RED_DRACONIAN:
        case SP_WHITE_DRACONIAN:
        case SP_GREEN_DRACONIAN:
        case SP_YELLOW_DRACONIAN:
        case SP_GREY_DRACONIAN:
        case SP_BLACK_DRACONIAN:
        case SP_PURPLE_DRACONIAN:
        case SP_MOTTLED_DRACONIAN:
        case SP_PALE_DRACONIAN:
        {
            tilep_draconian_init(sp, level, &result, &head, &wing);
            hair   = 0;
            break;
        }
        case SP_MINOTAUR:
            hair = 0;
            break;
        case SP_DEMONSPAWN:
            hair = 0;
            break;
        case SP_GHOUL:
            hair = 0;
            break;
        case SP_MERFOLK:
            result = you.in_water() ? TILEP_BASE_MERFOLK_WATER
                                    : TILEP_BASE_MERFOLK;
            hair = TILEP_HAIR_GREEN;
            beard = TILEP_BEARD_SHORT_GREEN;
            break;
        case SP_VAMPIRE:
            hair = TILEP_HAIR_ARWEN;
            break;
        case SP_DEEP_DWARF:
            hair  = TILEP_HAIR_SHORT_WHITE;
            beard = TILEP_BEARD_LONG_WHITE;
            break;
        case SP_SPRIGGAN:
            hair = 0;
            beard = TILEP_BEARD_LONG_GREEN;
            break;
        default:
            // nothing to do
            break;
    }

    parts[TILEP_PART_BASE] = result;

    // Don't overwrite doll parts defined elsewhere.
    if (parts[TILEP_PART_HAIR] == TILEP_SHOW_EQUIP)
        parts[TILEP_PART_HAIR] = hair;
    if (parts[TILEP_PART_BEARD] == TILEP_SHOW_EQUIP)
        parts[TILEP_PART_BEARD] = beard;
    if (parts[TILEP_PART_SHADOW] == TILEP_SHOW_EQUIP)
        parts[TILEP_PART_SHADOW] = TILEP_SHADOW_SHADOW;
    if (parts[TILEP_PART_DRCHEAD] == TILEP_SHOW_EQUIP)
        parts[TILEP_PART_DRCHEAD] = head;
    if (parts[TILEP_PART_DRCWING] == TILEP_SHOW_EQUIP)
        parts[TILEP_PART_DRCWING] = wing;
}

// This function needs to be entirely deterministic.
void tilep_job_default(int job, dolls_data *doll)
{
    tileidx_t *parts = doll->parts;

    parts[TILEP_PART_CLOAK] = TILEP_SHOW_EQUIP;
    parts[TILEP_PART_BOOTS] = TILEP_SHOW_EQUIP;
    parts[TILEP_PART_LEG]   = TILEP_SHOW_EQUIP;
    parts[TILEP_PART_BODY]  = TILEP_SHOW_EQUIP;
    parts[TILEP_PART_ARM]   = TILEP_SHOW_EQUIP;
    parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
    parts[TILEP_PART_HAND2] = TILEP_SHOW_EQUIP;
    parts[TILEP_PART_HELM]  = TILEP_SHOW_EQUIP;

    switch (job)
    {
        case JOB_FIGHTER:
            parts[TILEP_PART_LEG]   = TILEP_LEG_METAL_SILVER;
            break;

        case JOB_CRUSADER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_WHITE3;
            parts[TILEP_PART_LEG]   = TILEP_LEG_SKIRT_OFS;
            parts[TILEP_PART_HELM]  = TILEP_HELM_HELM_IRON;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_GRAY;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_GRAY;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLUE;
            break;

        case JOB_PALADIN:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_WHITE;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_HELM_IRON;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_GRAY;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_GRAY;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLUE;
            break;

        case JOB_CHAOS_KNIGHT:
            parts[TILEP_PART_BODY]  = TILEP_BODY_BELT1;
            parts[TILEP_PART_LEG]   = TILEP_LEG_METAL_GRAY;
            parts[TILEP_PART_HELM]  = TILEP_HELM_FHELM_PLUME;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_BERSERKER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ANIMAL_SKIN;
            parts[TILEP_PART_LEG]   = TILEP_LEG_BELT_REDBROWN;
            break;

        case JOB_REAVER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BLACK_GOLD;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BROWN;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_RED_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_STALKER:
            parts[TILEP_PART_HELM]  = TILEP_HELM_HOOD_GREEN;
            parts[TILEP_PART_BODY]  = TILEP_BODY_LEATHER_JACKET;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_SHORT_GRAY;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_SWORD_THIEF;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_GREEN_DIM;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_WRIST_PURPLE;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_GREEN;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_BROWN2;
            break;

        case JOB_ASSASSIN:
            parts[TILEP_PART_HELM]  = TILEP_HELM_MASK_NINJA_BLACK;
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_BLACK3;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BLACK;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_SWORD_THIEF;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_BLACK;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLACK;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN2;
            break;


        case JOB_WIZARD:
            parts[TILEP_PART_BODY]  = TILEP_BODY_GANDALF_G;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_GANDALF;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_CYAN_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_WIZARD_GRAY;
            break;

        case JOB_PRIEST:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_WHITE;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_WHITE;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_HEALER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_WHITE;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_WHITE;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_DAGGER;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_FHELM_HEALER;
            break;

        case JOB_NECROMANCER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BLACK;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_STAFF_SKULL;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_BLACK;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_FIRE_ELEMENTALIST:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_RED;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_GANDALF;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_RED_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_ICE_ELEMENTALIST:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BLUE;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_GANDALF;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_BLUE_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_AIR_ELEMENTALIST:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_CYAN;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_GANDALF;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_CYAN_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_EARTH_ELEMENTALIST:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_YELLOW;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_GANDALF;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_YELLOW_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_VENOM_MAGE:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_GREEN;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_GANDALF;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_GREEN_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_TRANSMUTER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_RAINBOW;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_STAFF_RUBY;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_MAGENTA_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_CONJURER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_MAGENTA;
            parts[TILEP_PART_HELM]  = TILEP_HELM_WIZARD_GRAY;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_STAFF_MAGE2;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_RED_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_ENCHANTER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_YELLOW;
            parts[TILEP_PART_HELM]  = TILEP_HELM_WIZARD_GRAY;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_STAFF_MAGE;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_BLUE_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_SUMMONER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_WIZARD_GRAY;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_STAFF_RING_BLUE;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_YELLOW_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_WARPER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_WIZARD_GRAY;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_SARUMAN;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_WHITE;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_RED;
            break;


        case JOB_ARCANE_MARKSMAN:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_WIZARD_GRAY;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_SARUMAN;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_WHITE;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_RED;
            break;

        case JOB_HUNTER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_LEATHER_ARMOUR2;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BROWN;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_BOW;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_BROWN;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_BROWN;
            break;

        case JOB_GLADIATOR:
            parts[TILEP_PART_HAND2] = TILEP_HAND2_SHIELD_ROUND2;
            parts[TILEP_PART_BODY]  = TILEP_BODY_BELT1;
            parts[TILEP_PART_LEG]   = TILEP_LEG_BELT_GRAY;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_GRAY;
            break;

        case JOB_MONK:
            parts[TILEP_PART_BODY]  = TILEP_BODY_MONK_BLACK;
            break;

        case JOB_WANDERER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_HAWAII;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_SHORT_BROWN;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_BROWN3;
            break;

        case JOB_ARTIFICER:
            parts[TILEP_PART_HAND1] = TILEP_HAND1_SCEPTRE;
            parts[TILEP_PART_BODY]  = TILEP_BODY_LEATHER_ARMOUR3;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BLACK;
            break;
    }
}

void tilep_calc_flags(const dolls_data &doll, int flag[])
{
    for (unsigned i = 0; i < TILEP_PART_MAX; i++)
        flag[i] = TILEP_FLAG_NORMAL;

    if (doll.parts[TILEP_PART_HELM] >= TILEP_HELM_HELM_OFS)
        flag[TILEP_PART_HAIR] = TILEP_FLAG_HIDE;

    if (doll.parts[TILEP_PART_HELM] >= TILEP_HELM_FHELM_OFS)
        flag[TILEP_PART_BEARD] = TILEP_FLAG_HIDE;

    if (is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_NAGA))
    {
        flag[TILEP_PART_BOOTS] = flag[TILEP_PART_LEG] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_CUT_NAGA;
    }
    else if (is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_CENTAUR))
    {
        flag[TILEP_PART_BOOTS] = flag[TILEP_PART_LEG] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_CUT_CENTAUR;
    }
    else if (is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_MERFOLK_WATER))
    {
        flag[TILEP_PART_BOOTS]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_LEG]    = TILEP_FLAG_HIDE;
        flag[TILEP_PART_SHADOW] = TILEP_FLAG_HIDE;
    }
    else if (doll.parts[TILEP_PART_BASE] >= TILEP_BASE_DRACONIAN_FIRST
             && doll.parts[TILEP_PART_BASE] <= TILEP_BASE_DRACONIAN_LAST)
    {
        flag[TILEP_PART_HAIR] = flag[TILEP_PART_HELM] = TILEP_FLAG_HIDE;
    }
    else if (is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_FELID))
    {
        flag[TILEP_PART_CLOAK] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BOOTS] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_LEG]   = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_ARM]   = TILEP_FLAG_HIDE;
        flag[TILEP_PART_HAND1] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_HAND2] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_HELM]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_HAIR]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BEARD] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_SHADOW]= TILEP_FLAG_HIDE;
        flag[TILEP_PART_DRCWING]=TILEP_FLAG_HIDE;
        flag[TILEP_PART_DRCHEAD]=TILEP_FLAG_HIDE;
    }
}

// Parts index to string
void tilep_part_to_str(int number, char *buf)
{
    //special
    if (number == TILEP_SHOW_EQUIP)
    {
        buf[0] = buf[1] = buf[2] = '*';
    }
    else
    {
        //normal 2 digits
        buf[0] = '0' + (number/100) % 10;
        buf[1] = '0' + (number/ 10) % 10;
        buf[2] = '0' +  number      % 10;
    }
    buf[3] = '\0';
}

// Parts string to index
int tilep_str_to_part(char *str)
{
    //special
    if (str[0] == '*')
        return TILEP_SHOW_EQUIP;

    //normal 3 digits
    return atoi(str);
}

// This order is to preserve dolls.txt integrity over multiple versions.
// Newer entries should be added to the end before the -1 terminator.
const int parts_saved[TILEP_PART_MAX + 1] =
{
    TILEP_PART_SHADOW,
    TILEP_PART_BASE,
    TILEP_PART_CLOAK,
    TILEP_PART_BOOTS,
    TILEP_PART_LEG,
    TILEP_PART_BODY,
    TILEP_PART_ARM,
    TILEP_PART_HAND1,
    TILEP_PART_HAND2,
    TILEP_PART_HAIR,
    TILEP_PART_BEARD,
    TILEP_PART_HELM,
    TILEP_PART_HALO,
    TILEP_PART_ENCH,
    TILEP_PART_DRCWING,
    TILEP_PART_DRCHEAD,
    -1
};

/*
 * scan input line from dolls.txt
 */
void tilep_scan_parts(char *fbuf, dolls_data &doll, int species, int level)
{
    char  ibuf[8];

    int gcount = 0;
    int ccount = 0;
    for (int i = 0; parts_saved[i] != -1; ++i)
    {
        ccount = 0;
        int p = parts_saved[i];

        while (fbuf[gcount] != ':' && fbuf[gcount] != '\n'
               && ccount < 4 && gcount < (i+1)*4)
        {
            ibuf[ccount++] = fbuf[gcount++];
        }

        ibuf[ccount] = '\0';
        gcount++;

        const tileidx_t idx = tilep_str_to_part(ibuf);
        if (idx == TILEP_SHOW_EQUIP)
            doll.parts[p] = TILEP_SHOW_EQUIP;
        else if (p == TILEP_PART_BASE)
        {
            const tileidx_t base_tile = tilep_species_to_base_tile(species, level);
            if (idx >= tile_player_count(base_tile))
                doll.parts[p] = base_tile;
            else
                doll.parts[p] = base_tile + idx;
        }
        else if (idx == 0)
            doll.parts[p] = 0;
        else if (idx > tile_player_part_count[p])
            doll.parts[p] = tile_player_part_start[p];
        else
        {
            const tileidx_t idx2 = tile_player_part_start[p] + idx - 1;
            if (idx2 < TILE_MAIN_MAX || idx2 >= TILEP_PLAYER_MAX)
                doll.parts[p] = TILEP_SHOW_EQUIP;
            else
                doll.parts[p] = idx2;
        }
    }
}

/*
 * format-print parts
 */
void tilep_print_parts(char *fbuf, const dolls_data &doll)
{
    char *ptr = fbuf;
    for (unsigned i = 0; parts_saved[i] != -1; ++i)
    {
        const int p = parts_saved[i];
        tileidx_t idx = doll.parts[p];
        if (idx != TILEP_SHOW_EQUIP)
        {
            if (p == TILEP_PART_BASE)
            {
                idx -= tilep_species_to_base_tile(you.species,
                                                  you.experience_level);
            }
            else if (idx != 0)
            {
                idx = doll.parts[p] - tile_player_part_start[p] + 1;
                if (idx < 0 || idx > tile_player_part_count[p])
                    idx = 0;
            }
        }
        tilep_part_to_str(idx, ptr);

        ptr += 3;

        *ptr = ':';
        ptr++;
    }
    ptr[0] = '\n'; // erase the last ':'
    ptr[1] = 0;
}

#endif

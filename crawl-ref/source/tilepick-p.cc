#include "AppHdr.h"

#ifdef USE_TILE
#include "tilepick-p.h"

#include <stdio.h>

#include "artefact.h"
#include "describe.h"
#include "itemname.h"
#include "itemprop.h"
#include "mon-stuff.h"
#include "player.h"
#include "tiledef-player.h"
#include "tiledef-unrand.h"
#include "tiledoll.h"
#include "tilepick.h"
#include "transform.h"
#include "traps.h"

static tileidx_t _modrng(int mod, tileidx_t first, tileidx_t last)
{
    return (first + mod % (last - first + 1));
}

static tileidx_t _mon_mod(tileidx_t tile, int offset)
{
    int count = tile_player_count(tile);
    return (tile + offset % count);
}

tileidx_t tilep_equ_weapon(const item_def &item)
{
    if (item.props.exists("worn_tile"))
        return item.props["worn_tile"].get_short();

    if (item.base_type == OBJ_STAVES)
    {
        int orig_special = you.item_description[IDESC_STAVES][item.sub_type];
        int desc = (orig_special / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;
        return TILEP_HAND1_STAFF_LARGE + desc;
    }

    if (item.base_type == OBJ_RODS)
        return _mon_mod(TILEP_HAND1_ROD_BROWN, item.rnd);

    if (item.base_type == OBJ_MISCELLANY)
    {
        switch (item.sub_type)
        {
        case MISC_BOTTLED_EFREET:             return TILEP_HAND1_BOTTLE;
        case MISC_FAN_OF_GALES:               return TILEP_HAND1_FAN;
        case MISC_STONE_OF_TREMORS:           return TILEP_HAND1_STONE;
        case MISC_DISC_OF_STORMS:             return TILEP_HAND1_DISC;

        case MISC_CRYSTAL_BALL_OF_ENERGY:     return TILEP_HAND1_CRYSTAL;

        case MISC_LAMP_OF_FIRE:               return TILEP_HAND1_LANTERN;
        case MISC_LANTERN_OF_SHADOWS:         return TILEP_HAND1_BONE_LANTERN;
        case MISC_HORN_OF_GERYON:             return TILEP_HAND1_HORN;
        case MISC_BOX_OF_BEASTS:              return TILEP_HAND1_BOX;

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

    tileidx_t tile = 0;

    switch (item.sub_type)
    {
    // Blunt
    case WPN_CLUB:
        tile = TILEP_HAND1_CLUB_SLANT;
        break;
    case WPN_MACE:
        tile = TILEP_HAND1_MACE;
        break;
    case WPN_GREAT_MACE:
        tile = TILEP_HAND1_GREAT_MACE;
        break;
    case WPN_FLAIL:
        tile = TILEP_HAND1_FLAIL;
        break;
    case WPN_DIRE_FLAIL:
        tile = TILEP_HAND1_GREAT_FLAIL;
        break;
    case WPN_MORNINGSTAR:
        tile = TILEP_HAND1_MORNINGSTAR;
        break;
    case WPN_EVENINGSTAR:
        tile = TILEP_HAND1_EVENINGSTAR;
        break;
    case WPN_GIANT_CLUB:
        tile = TILEP_HAND1_GIANT_CLUB_PLAIN;
        break;
    case WPN_GIANT_SPIKED_CLUB:
        tile = TILEP_HAND1_GIANT_CLUB_SPIKE_SLANT;
        break;
    case WPN_WHIP:
        tile = TILEP_HAND1_WHIP;
        break;
    case WPN_DEMON_WHIP:
        tile = TILEP_HAND1_BLACK_WHIP;
        break;
    case WPN_SACRED_SCOURGE:
        tile = TILEP_HAND1_SACRED_SCOURGE;
        break;

    // Edge
    case WPN_DAGGER:
        tile = TILEP_HAND1_DAGGER_SLANT;
        break;
    case WPN_SHORT_SWORD:
        tile = TILEP_HAND1_SHORT_SWORD_SLANT;
        break;
    case WPN_LONG_SWORD:
        tile = TILEP_HAND1_LONG_SWORD_SLANT;
        break;
    case WPN_GREAT_SWORD:
        tile = TILEP_HAND1_GREAT_SWORD_SLANT;
        break;
    case WPN_SCIMITAR:
        tile = TILEP_HAND1_SCIMITAR;
        break;
    case WPN_FALCHION:
        tile = TILEP_HAND1_FALCHION;
        break;
    case WPN_SABRE:
        tile = TILEP_HAND1_SABRE;
        break;
    case WPN_DEMON_BLADE:
        tile = TILEP_HAND1_DEMON_BLADE;
        break;
    case WPN_QUICK_BLADE:
        tile = TILEP_HAND1_DAGGER;
        break;
    case WPN_DOUBLE_SWORD:
        tile = TILEP_HAND1_DOUBLE_SWORD;
        break;
    case WPN_TRIPLE_SWORD:
        tile = TILEP_HAND1_TRIPLE_SWORD;
        break;
    case WPN_EUDEMON_BLADE:
        tile = TILEP_HAND1_BLESSED_BLADE;
        break;

    // new blessed weapons
    case WPN_BLESSED_LONG_SWORD:
        tile = TILEP_HAND1_LONG_SWORD_SLANT;
        break;
    case WPN_BLESSED_GREAT_SWORD:
        tile = TILEP_HAND1_GREAT_SWORD_SLANT;
        break;
    case WPN_BLESSED_SCIMITAR:
        tile = TILEP_HAND1_SCIMITAR;
        break;
    case WPN_BLESSED_FALCHION:
        tile = TILEP_HAND1_FALCHION;
        break;
    case WPN_BLESSED_DOUBLE_SWORD:
        tile = TILEP_HAND1_DOUBLE_SWORD;
        break;
    case WPN_BLESSED_TRIPLE_SWORD:
        tile = TILEP_HAND1_TRIPLE_SWORD;
        break;

    // Axe
    case WPN_HAND_AXE:
        tile = TILEP_HAND1_HAND_AXE;
        break;
    case WPN_BATTLEAXE:
        tile = TILEP_HAND1_BATTLEAXE;
        break;
    case WPN_BROAD_AXE:
        tile = TILEP_HAND1_BROAD_AXE;
        break;
    case WPN_WAR_AXE:
        tile = TILEP_HAND1_WAR_AXE;
        break;
    case WPN_EXECUTIONERS_AXE:
        tile = TILEP_HAND1_EXECUTIONERS_AXE;
        break;
    case WPN_BARDICHE:
        tile = TILEP_HAND1_GLAIVE3;
        break;

    // Pole
    case WPN_SPEAR:
        tile = TILEP_HAND1_SPEAR;
        break;
    case WPN_HALBERD:
        tile = TILEP_HAND1_HALBERD;
        break;
    case WPN_GLAIVE:
        tile = TILEP_HAND1_GLAIVE;
        break;
#if TAG_MAJOR_VERSION == 34
    case WPN_STAFF:
        tile = TILEP_HAND1_STAFF;
        break;
#endif
    case WPN_QUARTERSTAFF:
        tile = TILEP_HAND1_QUARTERSTAFF1;
        break;
    case WPN_LAJATANG:
        tile = TILEP_HAND1_LAJATANG;
        break;

    case WPN_SCYTHE:
        tile = TILEP_HAND1_SCYTHE;
        break;
    case WPN_HAMMER:
        tile = TILEP_HAND1_HAMMER;
        break;
    case WPN_TRIDENT:
        tile = TILEP_HAND1_TRIDENT2;
        break;
    case WPN_DEMON_TRIDENT:
        tile = TILEP_HAND1_DEMON_TRIDENT;
        break;
    case WPN_TRISHULA:
        tile = TILEP_HAND1_TRISHULA;
        break;

    // Ranged
    case WPN_SLING:
        tile = TILEP_HAND1_SLING;
        break;
    case WPN_BOW:
        tile = TILEP_HAND1_BOW2;
        break;
    case WPN_CROSSBOW:
        tile = TILEP_HAND1_CROSSBOW;
        break;
    case WPN_BLOWGUN:
        tile = TILEP_HAND1_BLOWGUN;
        break;
    case WPN_LONGBOW:
        tile = TILEP_HAND1_BOW3;
        break;

    default: tile = 0;
    }

    return tile ? tileidx_enchant_equ(item, tile, true) : 0;
}

tileidx_t tilep_equ_shield(const item_def &item)
{
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (item.props.exists("worn_tile"))
        return item.props["worn_tile"].get_short();

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    switch (item.sub_type)
    {
        case ARM_SHIELD:
            return _modrng(item.rnd, TILEP_HAND2_SHIELD_FIRST_NORM,
                           TILEP_HAND2_SHIELD_LAST_NORM);
        case ARM_BUCKLER:
            return _modrng(item.rnd, TILEP_HAND2_BUCKLER_FIRST_NORM,
                           TILEP_HAND2_BUCKLER_LAST_NORM);
        case ARM_LARGE_SHIELD:
            return _modrng(item.rnd, TILEP_HAND2_LSHIELD_FIRST_NORM,
                           TILEP_HAND2_LSHIELD_LAST_NORM);
        default: return 0;
    }
}

tileidx_t tilep_equ_armour(const item_def &item)
{
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (item.props.exists("worn_tile"))
        return item.props["worn_tile"].get_short();

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
    case ARM_PLATE_ARMOUR:       return TILEP_BODY_PLATE_BLACK;
    case ARM_CRYSTAL_PLATE_ARMOUR:
        return tileidx_enchant_equ(item, TILEP_BODY_CRYSTAL_PLATE, true);

    case ARM_FIRE_DRAGON_HIDE:    return TILEP_BODY_DRAGONSC_GREEN;
    case ARM_ICE_DRAGON_HIDE:     return TILEP_BODY_DRAGONSC_CYAN;
    case ARM_STEAM_DRAGON_HIDE:   return TILEP_BODY_DRAGONSC_WHITE;
    case ARM_MOTTLED_DRAGON_HIDE: return TILEP_BODY_DRAGONSC_MAGENTA;
    case ARM_STORM_DRAGON_HIDE:   return TILEP_BODY_DRAGONSC_BLUE;
    case ARM_GOLD_DRAGON_HIDE:    return TILEP_BODY_DRAGONSC_GOLD;
    case ARM_SWAMP_DRAGON_HIDE:   return TILEP_BODY_DRAGONSC_BROWN;
    case ARM_PEARL_DRAGON_HIDE:   return TILEP_BODY_DRAGONSC_PEARL;

    case ARM_FIRE_DRAGON_ARMOUR:    return TILEP_BODY_DRAGONARM_GREEN;
    case ARM_ICE_DRAGON_ARMOUR:     return TILEP_BODY_DRAGONARM_CYAN;
    case ARM_STEAM_DRAGON_ARMOUR:   return TILEP_BODY_DRAGONARM_WHITE;
    case ARM_MOTTLED_DRAGON_ARMOUR: return TILEP_BODY_DRAGONARM_MAGENTA;
    case ARM_STORM_DRAGON_ARMOUR:   return TILEP_BODY_DRAGONARM_BLUE;
    case ARM_GOLD_DRAGON_ARMOUR:    return TILEP_BODY_DRAGONARM_GOLD;
    case ARM_SWAMP_DRAGON_ARMOUR:   return TILEP_BODY_DRAGONARM_BROWN;
    case ARM_PEARL_DRAGON_ARMOUR:   return TILEP_BODY_DRAGONARM_PEARL;

    case ARM_ANIMAL_SKIN:          return TILEP_BODY_ANIMAL_SKIN;
    case ARM_TROLL_HIDE:
    case ARM_TROLL_LEATHER_ARMOUR: return TILEP_BODY_TROLL_HIDE;

    default: return 0;
    }
}

tileidx_t tilep_equ_cloak(const item_def &item)
{
    if (item.base_type != OBJ_ARMOUR || item.sub_type != ARM_CLOAK)
        return 0;

    if (item.props.exists("worn_tile"))
        return item.props["worn_tile"].get_short();

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
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (item.props.exists("worn_tile"))
        return item.props["worn_tile"].get_short();

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
    if (item.base_type != OBJ_ARMOUR || item.sub_type != ARM_GLOVES)
        return 0;

    if (item.props.exists("worn_tile"))
        return item.props["worn_tile"].get_short();

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
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (item.props.exists("worn_tile"))
        return item.props["worn_tile"].get_short();

    int etype = enchant_to_int(item);

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    if (item.sub_type == ARM_NAGA_BARDING)
        return TILEP_BOOTS_NAGA_BARDING + min(etype, 3);

    if (item.sub_type == ARM_CENTAUR_BARDING)
        return TILEP_BOOTS_CENTAUR_BARDING + min(etype, 3);

    if (item.sub_type != ARM_BOOTS)
        return 0;

    return _modrng(item.rnd, TILEP_BOOTS_FIRST_NORM, TILEP_BOOTS_LAST_NORM);
}

tileidx_t tileidx_player()
{
    int ch = TILEP_PLAYER;

    // Handle shapechange first
    switch (you.form)
    {
        // equipment-using forms are handled regularly
        case TRAN_STATUE:
        case TRAN_LICH:
        case TRAN_TREE:
            break;
        // animals
        case TRAN_BAT:       ch = TILEP_TRAN_BAT;       break;
        case TRAN_SPIDER:    ch = TILEP_TRAN_SPIDER;    break;
        case TRAN_PIG:       ch = TILEP_TRAN_PIG;       break;
        case TRAN_PORCUPINE: ch = TILEP_MONS_PORCUPINE; break;
        // non-animals
        case TRAN_ICE_BEAST: ch = TILEP_TRAN_ICE_BEAST; break;
        case TRAN_WISP:      ch = TILEP_MONS_INSUBSTANTIAL_WISP; break;
        case TRAN_JELLY:     ch = TILEP_MONS_JELLY;     break;
        case TRAN_FUNGUS:    ch = TILEP_MONS_WANDERING_MUSHROOM; break;
        case TRAN_DRAGON:
        {
            switch (you.species)
            {
            case SP_BLACK_DRACONIAN:   ch = TILEP_TRAN_DRAGON_BLACK;   break;
            case SP_YELLOW_DRACONIAN:  ch = TILEP_TRAN_DRAGON_YELLOW;  break;
            case SP_GREY_DRACONIAN:    ch = TILEP_TRAN_DRAGON_GREY;    break;
            case SP_GREEN_DRACONIAN:   ch = TILEP_TRAN_DRAGON_GREEN;   break;
            case SP_MOTTLED_DRACONIAN: ch = TILEP_TRAN_DRAGON_MOTTLED; break;
            case SP_PALE_DRACONIAN:    ch = TILEP_TRAN_DRAGON_PALE;    break;
            case SP_PURPLE_DRACONIAN:  ch = TILEP_TRAN_DRAGON_PURPLE;  break;
            case SP_WHITE_DRACONIAN:   ch = TILEP_TRAN_DRAGON_WHITE;   break;
            case SP_RED_DRACONIAN:     ch = TILEP_TRAN_DRAGON_RED;     break;
            default:                   ch = TILEP_TRAN_DRAGON;         break;
            }
            break;
        }
        // no special tile
        case TRAN_BLADE_HANDS: break;
        case TRAN_APPENDAGE:
        case TRAN_NONE:
        default:
            break;
    }

    // Currently, the flying flag is only used for not drawing the tile in the
    // water. in_water() checks Beogh's water walking. If the flying flag is
    // used for something else, we would need to add an in_water flag.
    if (!you.in_water())
        ch |= TILE_FLAG_FLYING;

    if (you.attribute[ATTR_HELD])
    {
        if (get_trapping_net(you.pos()) == NON_ITEM)
            ch |= TILE_FLAG_WEB;
        else
            ch |= TILE_FLAG_NET;
    }

    if (you.duration[DUR_POISONING])
        ch |= TILE_FLAG_POISON;

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
        case MONS_DRACONIAN:        return 0;
        case MONS_BLACK_DRACONIAN:  return 1;
        case MONS_YELLOW_DRACONIAN: return 2;
        case MONS_GREY_DRACONIAN:   return 3;
        case MONS_GREEN_DRACONIAN:  return 4;
        case MONS_MOTTLED_DRACONIAN:return 5;
        case MONS_PALE_DRACONIAN:   return 6;
        case MONS_PURPLE_DRACONIAN: return 7;
        case MONS_RED_DRACONIAN:    return 8;
        case MONS_WHITE_DRACONIAN:  return 9;
        }
    }
    switch (race)
    {
    case SP_BLACK_DRACONIAN:   return 1;
    case SP_YELLOW_DRACONIAN:  return 2;
    case SP_GREY_DRACONIAN:    return 3;
    case SP_GREEN_DRACONIAN:   return 4;
    case SP_MOTTLED_DRACONIAN: return 5;
    case SP_PALE_DRACONIAN:    return 6;
    case SP_PURPLE_DRACONIAN:  return 7;
    case SP_RED_DRACONIAN:     return 8;
    case SP_WHITE_DRACONIAN:   return 9;
    }
    return 0;
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
    case SP_LAVA_ORC:
        return TILEP_BASE_LAVA_ORC;
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
    case SP_TENGU:
        return TILEP_BASE_TENGU;
    case SP_MERFOLK:
        return TILEP_BASE_MERFOLK;
    case SP_VAMPIRE:
        return TILEP_BASE_VAMPIRE;
    case SP_DEEP_DWARF:
        return TILEP_BASE_DEEP_DWARF;
    case SP_GARGOYLE:
        return TILEP_BASE_GARGOYLE;
    case SP_FELID:
        return TILEP_BASE_FELID;
    case SP_OCTOPODE:
        return TILEP_BASE_OCTOPODE;
    case SP_DJINNI:
        return TILEP_BASE_DJINNI;
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
            beard = TILEP_BEARD_FULL_RED;
            break;
        case SP_HILL_ORC:
            hair = 0;
            break;
        case SP_LAVA_ORC:
            // This should respect the player's choice of base tile, if possible.
            switch (temperature_colour(you.temperature))
            {
                case LIGHTRED:
                    result = TILEP_BASE_LAVA_ORC_HEAT + 5;
                    break;
                case RED:
                    result = TILEP_BASE_LAVA_ORC_HEAT + 4;
                    break;
                case YELLOW:
                    result = TILEP_BASE_LAVA_ORC_HEAT + 3;
                    break;
                case WHITE:
                    result = TILEP_BASE_LAVA_ORC_HEAT + 2;
                    break;
                case LIGHTCYAN:
                    result = TILEP_BASE_LAVA_ORC_HEAT + 1;
                    break;
                case LIGHTBLUE:
                    result = TILEP_BASE_LAVA_ORC_HEAT;
                    break;
                default:
                    result = TILEP_BASE_LAVA_ORC;
                    break;
            }
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
            result = you.fishtail ? TILEP_BASE_MERFOLK_WATER
                                  : TILEP_BASE_MERFOLK;
            hair = TILEP_HAIR_GREEN;
            beard = TILEP_BEARD_SHORT_GREEN;
            break;
        case SP_VAMPIRE:
            hair = TILEP_HAIR_ARWEN;
            break;
        case SP_DEEP_DWARF:
            hair  = TILEP_HAIR_SHORT_WHITE;
            beard = TILEP_BEARD_GARIBALDI_WHITE;
            break;
        case SP_SPRIGGAN:
            hair = 0;
            beard = TILEP_BEARD_MEDIUM_GREEN;
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

        case JOB_SKALD:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_WHITE3;
            parts[TILEP_PART_LEG]   = TILEP_LEG_SKIRT_OFS;
            parts[TILEP_PART_HELM]  = TILEP_HELM_HELM_IRON;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_GRAY;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_GRAY;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLUE;
            break;

        case JOB_CHAOS_KNIGHT:
            parts[TILEP_PART_BODY]  = TILEP_BODY_MESH_BLACK;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_SHORT_DARKBROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_CLOWN; // Xom
            break;

        case JOB_DEATH_KNIGHT:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_BLACK3;
            parts[TILEP_PART_LEG]   = TILEP_LEG_METAL_GRAY;
            parts[TILEP_PART_HELM]  = TILEP_HELM_FHELM_OFS;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_BLACK;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_ABYSSAL_KNIGHT:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHOULDER_PAD;
            parts[TILEP_PART_LEG]   = TILEP_LEG_METAL_GRAY;
            parts[TILEP_PART_HELM]  = TILEP_HELM_FHELM_PLUME;
            break;

        case JOB_BERSERKER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ANIMAL_SKIN;
            parts[TILEP_PART_LEG]   = TILEP_LEG_BELT_REDBROWN;
            break;

#if TAG_MAJOR_VERSION == 34
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
#endif

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

#if TAG_MAJOR_VERSION == 34
        case JOB_PRIEST:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_WHITE;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_WHITE;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;
#endif

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
            parts[TILEP_PART_BODY]  = TILEP_BODY_LEGOLAS;
            parts[TILEP_PART_HELM]  = TILEP_HELM_FEATHER_GREEN;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_DARKGREEN;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_BROWN3;
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
    else if (is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_DJINNI))
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
        if (!is_player_tile(doll.parts[TILEP_PART_HELM], TILEP_HELM_HORNS_CAT))
            flag[TILEP_PART_HELM] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_HAIR]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BEARD] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_SHADOW]= TILEP_FLAG_HIDE;
        flag[TILEP_PART_DRCWING]=TILEP_FLAG_HIDE;
        flag[TILEP_PART_DRCHEAD]=TILEP_FLAG_HIDE;
    }
    else if (is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_OCTOPODE))
    {
        flag[TILEP_PART_CLOAK] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BOOTS] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_LEG]   = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_HIDE;
        if (doll.parts[TILEP_PART_ARM] != TILEP_ARM_OCTOPODE_SPIKE)
            flag[TILEP_PART_ARM] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_HAIR]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BEARD] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_SHADOW]= TILEP_FLAG_HIDE;
        flag[TILEP_PART_DRCWING]=TILEP_FLAG_HIDE;
        flag[TILEP_PART_DRCHEAD]=TILEP_FLAG_HIDE;
    }

    if (doll.parts[TILEP_PART_ARM] == TILEP_ARM_OCTOPODE_SPIKE
        && !is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_OCTOPODE))
    {
        flag[TILEP_PART_ARM] = TILEP_FLAG_HIDE;
    }
    if (is_player_tile(doll.parts[TILEP_PART_HELM], TILEP_HELM_HORNS_CAT)
        && (!is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_FELID)
            // Every felid tile has its own horns.
            || doll.parts[TILEP_PART_BASE] - TILEP_BASE_FELID
               != doll.parts[TILEP_PART_HELM] - TILEP_HELM_HORNS_CAT))
    {
        flag[TILEP_PART_ARM] = TILEP_FLAG_HIDE;
    }
}

// Parts index to string
static void _tilep_part_to_str(int number, char *buf)
{
    //special
    if (number == TILEP_SHOW_EQUIP)
        buf[0] = buf[1] = buf[2] = '*';
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
static int _tilep_str_to_part(char *str)
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

        const tileidx_t idx = _tilep_str_to_part(ibuf);
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
                ASSERT(idx >= 0);
                if (idx > tile_player_part_count[p])
                    idx = 0;
            }
        }
        _tilep_part_to_str(idx, ptr);

        ptr += 3;

        *ptr = ':';
        ptr++;
    }
    ptr[0] = '\n'; // erase the last ':'
    ptr[1] = 0;
}

#endif

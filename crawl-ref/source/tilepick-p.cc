#include "AppHdr.h"

#ifdef USE_TILE
#include "tilepick-p.h"

#include <cstdio>

#include "artefact.h"
#include "describe.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "player.h"
#include "tile-flags.h"
#include "tile-player-flag-cut.h"
#include "rltiles/tiledef-player.h"
#include "rltiles/tiledef-unrand.h"
#include "tag-version.h"
#include "tiledoll.h"
#include "tilepick.h"
#include "transform.h"
#include "traps.h"

static tileidx_t _modrng(int mod, tileidx_t first, tileidx_t last)
{
    return first + mod % (last - first + 1);
}

static tileidx_t _part_start(int p)
{
    if (p != TILEP_PART_HAND2)
        return tile_player_part_start[p];
    return tile_player_part_start[TILEP_PART_HAND1_MIRROR];
}

static unsigned int _part_count(int p)
{
    const unsigned int count = tile_player_part_count[p];
    if (p != TILEP_PART_HAND2)
        return count;
    return count + tile_player_part_count[TILEP_PART_HAND1_MIRROR];
}

tileidx_t tilep_equ_weapon(const item_def &item)
{
    if (item.base_type == OBJ_STAVES)
    {
        if (is_artefact(item))
        {
            const int off = item.rnd %
                            tile_player_count(TILEP_HAND1_STAFF_RANDART_OFFSET);
            return TILEP_HAND1_STAFF_RANDART_OFFSET + off;
        }
        int orig_special = you.item_description[IDESC_STAVES][item.sub_type];
        int desc = (orig_special / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;
        return TILEP_HAND1_STAFF_LARGE + desc;
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
    case WPN_RAPIER:
        tile = TILEP_HAND1_RAPIER;
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
    case WPN_SHORTBOW:
        tile = TILEP_HAND1_SHORTBOW;
        break;
    case WPN_ORCBOW:
        tile = TILEP_HAND1_ORCBOW;
        break;
    case WPN_HAND_CANNON:
        tile = TILEP_HAND1_HAND_CANNON;
        break;
    case WPN_ARBALEST:
        tile = TILEP_HAND1_ARBALEST;
        break;
    case WPN_TRIPLE_CROSSBOW:
        tile = TILEP_HAND1_TRIPLE_CROSSBOW;
        break;
    case WPN_LONGBOW:
        tile = TILEP_HAND1_ORCBOW;
        break;
#if TAG_MAJOR_VERSION == 34
    case WPN_BLOWGUN:
#endif
    default: tile = 0;
    }

    if (item.props.exists(WORN_TILE_KEY))
        tile = item.props[WORN_TILE_KEY].get_short();

    return tile ? tileidx_enchant_equ(item, tile, true) : 0;
}

tileidx_t tilep_equ_shield(const item_def &item)
{
    if (is_weapon(item) && you.has_mutation(MUT_WIELD_OFFHAND))
        return tilep_equ_weapon(item);

    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (item.props.exists(WORN_TILE_KEY))
        return item.props[WORN_TILE_KEY].get_short();

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    switch (item.sub_type)
    {
        case ARM_KITE_SHIELD:
            return _modrng(item.rnd, TILEP_HAND2_KITE_SHIELD_FIRST_NORM,
                           TILEP_HAND2_KITE_SHIELD_LAST_NORM);
        case ARM_BUCKLER:
            return _modrng(item.rnd, TILEP_HAND2_BUCKLER_FIRST_NORM,
                           TILEP_HAND2_BUCKLER_LAST_NORM);
        case ARM_TOWER_SHIELD:
            return _modrng(item.rnd, TILEP_HAND2_TOWER_SHIELD_FIRST_NORM,
                           TILEP_HAND2_TOWER_SHIELD_LAST_NORM);
        case ARM_ORB:
            return _modrng(item.rnd, TILEP_HAND2_ORB_FIRST,
                           TILEP_HAND2_ORB_LAST);
        default: return 0;
    }
}

tileidx_t mirror_weapon(const item_def &weapon)
{
    const tileidx_t unmirrored = tilep_equ_weapon(weapon);
    if (unmirrored < TILEP_HAND1_FIRST || unmirrored > TILEP_HAND1_LAST)
        return 0;
    return unmirrored - TILEP_HAND1_FIRST + TILEP_HAND1_MIRROR_FIRST;
}

tileidx_t tilep_equ_armour(const item_def &item)
{
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (item.props.exists(WORN_TILE_KEY))
        return item.props[WORN_TILE_KEY].get_short();

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    if (item.sub_type == ARM_ROBE)
    {
        return _modrng(item.rnd, TILEP_BODY_ROBE_FIRST_NORM,
                       TILEP_BODY_ROBE_LAST_NORM);
    }

    tileidx_t tile = 0;
    switch (item.sub_type)
    {
    case ARM_LEATHER_ARMOUR:        tile = TILEP_BODY_LEATHER_ARMOUR; break;
    case ARM_RING_MAIL:             tile = TILEP_BODY_RINGMAIL; break;
    case ARM_CHAIN_MAIL:            tile = TILEP_BODY_CHAINMAIL; break;
    case ARM_SCALE_MAIL:            tile = TILEP_BODY_SCALEMAIL; break;
    case ARM_PLATE_ARMOUR:          tile = TILEP_BODY_PLATE; break;
    case ARM_CRYSTAL_PLATE_ARMOUR:  tile = TILEP_BODY_CRYSTAL_PLATE; break;

    case ARM_FIRE_DRAGON_ARMOUR:    tile = TILEP_BODY_DRAGONARM_RED; break;
    case ARM_ICE_DRAGON_ARMOUR:     tile = TILEP_BODY_DRAGONARM_CYAN; break;
    case ARM_STEAM_DRAGON_ARMOUR:   tile = TILEP_BODY_DRAGONARM_WHITE; break;
    case ARM_ACID_DRAGON_ARMOUR:    tile = TILEP_BODY_DRAGONARM_YELLOW; break;
    case ARM_QUICKSILVER_DRAGON_ARMOUR: tile = TILEP_BODY_DRAGONARM_QUICKSILVER; break;
    case ARM_STORM_DRAGON_ARMOUR:   tile = TILEP_BODY_DRAGONARM_BLUE; break;
    case ARM_SHADOW_DRAGON_ARMOUR:  tile = TILEP_BODY_DRAGONARM_SHADOW; break;
    case ARM_GOLD_DRAGON_ARMOUR:    tile = TILEP_BODY_DRAGONARM_GOLD; break;
    case ARM_SWAMP_DRAGON_ARMOUR:   tile = TILEP_BODY_DRAGONARM_BROWN; break;
    case ARM_PEARL_DRAGON_ARMOUR:   tile = TILEP_BODY_DRAGONARM_PEARL; break;

    case ARM_ANIMAL_SKIN:           tile = TILEP_BODY_ANIMAL_SKIN; break;
    case ARM_TROLL_LEATHER_ARMOUR:  tile = TILEP_BODY_TROLL_LEATHER; break;

    default:                        tile = 0;
    }

    return tileidx_enchant_equ(item, tile, true);
}

tileidx_t tilep_equ_cloak(const item_def &item)
{
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (item.props.exists(WORN_TILE_KEY))
        return item.props[WORN_TILE_KEY].get_short();

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    switch (item.sub_type)
    {
        case ARM_CLOAK:
            return _modrng(item.rnd, TILEP_CLOAK_FIRST_NORM,
                           TILEP_CLOAK_LAST_NORM);

        case ARM_SCARF:
            return _modrng(item.rnd, TILEP_CLOAK_SCARF_FIRST_NORM,
                           TILEP_CLOAK_SCARF_LAST_NORM);
    }

    return 0;
}

tileidx_t tilep_equ_helm(const item_def &item)
{
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (item.props.exists(WORN_TILE_KEY))
        return item.props[WORN_TILE_KEY].get_short();

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;

        // Although there shouldn't be any, just in case
        // unhandled artefacts fall through to defaults...
    }

    switch (item.sub_type)
    {
#if TAG_MAJOR_VERSION == 34
        case ARM_CAP:
#endif
        case ARM_HAT:
        {
            auto equip_tile = tileidx_enchant_equ(item, TILE_THELM_HAT, false);
            if (item.props.exists(ITEM_TILE_KEY))
                equip_tile = item.props[ITEM_TILE_KEY].get_short();
            switch (equip_tile)
            {
            case TILE_THELM_ARCHER:
                return TILEP_HELM_ARCHER;
            case TILE_THELM_ARCHER2:
                return TILEP_HELM_ARCHER2;
            case TILE_THELM_HAT_EXPLORER:
                return TILEP_HELM_EXPLORER;
            case TILE_THELM_HAT_EXPLORER2:
                return TILEP_HELM_EXPLORER2;
            case TILE_THELM_HAT_SANTA:
                return TILEP_HELM_SANTA;
            default:
                return _modrng(item.rnd, TILEP_HELM_HAT_FIRST_NORM,
                               TILEP_HELM_HAT_LAST_NORM);
            }
        }

        case ARM_HELMET:
            return _modrng(item.rnd, TILEP_HELM_FIRST_NORM,
                           TILEP_HELM_LAST_NORM);
    }

    return 0;
}

tileidx_t tilep_equ_gloves(const item_def &item)
{
    if (item.base_type != OBJ_ARMOUR || item.sub_type != ARM_GLOVES)
        return 0;

    if (item.props.exists(WORN_TILE_KEY))
        return item.props[WORN_TILE_KEY].get_short();

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

    if (item.props.exists(WORN_TILE_KEY))
        return item.props[WORN_TILE_KEY].get_short();

    if (is_unrandom_artefact(item))
    {
        const tileidx_t tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile)
            return tile;
    }

    if (item.sub_type == ARM_BARDING)
    {
        if (is_artefact(item))
            return TILEP_BOOTS_BARDING_RANDART;
        if (item.flags & ISFLAG_COSMETIC_MASK)
            return TILEP_BOOTS_BARDING_EGO;
        return TILEP_BOOTS_BARDING;
    }

    if (item.sub_type != ARM_BOOTS)
        return 0;

    return _modrng(item.rnd, TILEP_BOOTS_FIRST_NORM, TILEP_BOOTS_LAST_NORM);
}

tileidx_t tileidx_player()
{
    tileidx_t ch = TILEP_PLAYER;

    // Handle shapechange first
    switch (you.form)
    {
    // equipment-using forms are handled regularly
    case transformation::beast:
    case transformation::flux:
    case transformation::maw:
    case transformation::statue:
    case transformation::death:
    case transformation::tree:
    // (so is storm form)
    case transformation::storm:
        break;
    // animals
    case transformation::bat:       ch = TILEP_TRAN_BAT;       break;
#if TAG_MAJOR_VERSION == 34
    case transformation::spider:    ch = TILEP_TRAN_SPIDER;    break;
    case transformation::porcupine:
#endif
    case transformation::pig:       ch = TILEP_TRAN_PIG;       break;
    // non-animals
    case transformation::serpent:   ch = TILEP_TRAN_SERPENT;   break;
    case transformation::wisp:      ch = TILEP_MONS_INSUBSTANTIAL_WISP; break;
#if TAG_MAJOR_VERSION == 34
    case transformation::jelly:     ch = TILEP_MONS_JELLY;     break;
#endif
    case transformation::fungus:    ch = TILEP_TRAN_MUSHROOM;  break;
    case transformation::dragon:
    {
        switch (you.species)
        {
        case SP_OCTOPODE:          ch = TILEP_TRAN_DRAGON_OCTOPODE; break;
        case SP_FELID:             ch = TILEP_TRAN_DRAGON_FELID;    break;
        case SP_BLACK_DRACONIAN:   ch = TILEP_TRAN_DRAGON_BLACK;    break;
        case SP_YELLOW_DRACONIAN:  ch = TILEP_TRAN_DRAGON_YELLOW;   break;
        case SP_GREY_DRACONIAN:    ch = TILEP_TRAN_DRAGON_GREY;     break;
        case SP_GREEN_DRACONIAN:   ch = TILEP_TRAN_DRAGON_GREEN;    break;
        case SP_PALE_DRACONIAN:    ch = TILEP_TRAN_DRAGON_PALE;     break;
        case SP_PURPLE_DRACONIAN:  ch = TILEP_TRAN_DRAGON_PURPLE;   break;
        case SP_WHITE_DRACONIAN:   ch = TILEP_TRAN_DRAGON_WHITE;    break;
        case SP_RED_DRACONIAN:
        default:                   ch = TILEP_TRAN_DRAGON;          break;
        }
        break;
    }
    // no special tile
    case transformation::blade_hands:
    case transformation::none:
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
    {
        int pois_perc = (you.hp <= 0) ? 100
                                  : ((you.hp - max(0, poison_survival())) * 100 / you.hp);
        if (pois_perc >= 100)
            ch |= TILE_FLAG_MAX_POISON;
        else if (pois_perc >= 35)
            ch |= TILE_FLAG_MORE_POISON;
        else
            ch |= TILE_FLAG_POISON;
    }

    return ch;
}

bool is_player_tile(tileidx_t tile, tileidx_t base_tile)
{
    return tile >= base_tile
           && tile < base_tile + tile_player_count(base_tile);
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
        case MONS_PALE_DRACONIAN:   return 5;
        case MONS_PURPLE_DRACONIAN: return 6;
        case MONS_RED_DRACONIAN:    return 7;
        case MONS_WHITE_DRACONIAN:  return 8;
        }
    }
    switch (race)
    {
    case SP_BLACK_DRACONIAN:   return 1;
    case SP_YELLOW_DRACONIAN:  return 2;
    case SP_GREY_DRACONIAN:    return 3;
    case SP_GREEN_DRACONIAN:   return 4;
    case SP_PALE_DRACONIAN:    return 5;
    case SP_PURPLE_DRACONIAN:  return 6;
    case SP_RED_DRACONIAN:     return 7;
    case SP_WHITE_DRACONIAN:   return 8;
    }
    return 0;
}

tileidx_t tilep_species_to_base_tile(int sp, int level)
{
    switch (sp)
    {
    case SP_HUMAN:
        return TILEP_BASE_HUMAN;
#if TAG_MAJOR_VERSION == 34
    case SP_HIGH_ELF:
    case SP_SLUDGE_ELF:
#endif
    case SP_DEEP_ELF:
        return TILEP_BASE_DEEP_ELF;
#if TAG_MAJOR_VERSION == 34
    case SP_HALFLING:
        return TILEP_BASE_HALFLING;
    case SP_HILL_ORC:
        return TILEP_BASE_ORC;
#endif
    case SP_KOBOLD:
        return TILEP_BASE_KOBOLD;
    case SP_MUMMY:
        return TILEP_BASE_MUMMY;
    case SP_NAGA:
        return TILEP_BASE_NAGA;
    case SP_ONI:
        return TILEP_BASE_ONI;
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
    case SP_PALE_DRACONIAN:
        return TILEP_BASE_DRACONIAN + _draconian_colour(sp, level);
    case SP_ARMATAUR:
#if TAG_MAJOR_VERSION == 34
    case SP_CENTAUR:
#endif
        return TILEP_BASE_ARMATAUR;
#if TAG_MAJOR_VERSION == 34
    case SP_METEORAN:
        return TILEP_BASE_METEORAN;
#endif
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
#if TAG_MAJOR_VERSION == 34
    case SP_MAYFLYTAUR:
#endif
        return TILEP_BASE_TENGU;
    case SP_MERFOLK:
        return TILEP_BASE_MERFOLK;
    case SP_VAMPIRE:
        return TILEP_BASE_VAMPIRE;
    case SP_GARGOYLE:
        return TILEP_BASE_GARGOYLE;
    case SP_FELID:
        return TILEP_BASE_FELID;
    case SP_OCTOPODE:
        return TILEP_BASE_OCTOPODE;
    case SP_FORMICID:
        return TILEP_BASE_FORMICID;
    case SP_VINE_STALKER:
        return TILEP_BASE_VINE_STALKER;
    case SP_BARACHI:
        return TILEP_BASE_BARACHI;
    case SP_GNOLL:
        return TILEP_BASE_GNOLL;
    case SP_DJINNI:
        return TILEP_BASE_DJINNI;
    case SP_COGLIN:
        return TILEP_BASE_COGLIN;
    default:
        return TILEP_BASE_HUMAN;
    }
}
void tilep_draconian_init(int sp, int level, tileidx_t *base, tileidx_t *wing)
{
    const int colour_offset = _draconian_colour(sp, level);
    *base = TILEP_BASE_DRACONIAN + colour_offset;

    if (you.has_mutation(MUT_BIG_WINGS))
        *wing = _part_start(TILEP_PART_DRCWING) + colour_offset;
}

static const string DOLL_BASE_KEY = "doll_base";

void randomize_doll_base()
{
    const tileidx_t base = tilep_species_to_base_tile(you.species,
                                                      you.experience_level);
    const int count = tile_player_count(base);
    const int rand_base = base + random2(count);
    you.props[DOLL_BASE_KEY] = rand_base;

}

// Set default parts of each race: body + optional beard, hair, etc.
// This function needs to be entirely deterministic.
void tilep_race_default(int sp, int level, dolls_data *doll)
{
    tileidx_t *parts = doll->parts;

    tileidx_t result = tilep_species_to_base_tile(sp, level);
    if (level == you.experience_level && you.props.exists(DOLL_BASE_KEY))
    {
        const int rand_doll = you.props[DOLL_BASE_KEY].get_int();
#if TAG_MAJOR_VERSION == 34
        if (is_player_tile(rand_doll, result))
#endif
            result = rand_doll;
    }
    if (parts[TILEP_PART_BASE] != TILEP_SHOW_EQUIP)
        result = parts[TILEP_PART_BASE];

    tileidx_t hair   = 0;
    tileidx_t beard  = 0;
    tileidx_t wing   = 0;

    hair = TILEP_HAIR_SHORT_BLACK;

    switch (sp)
    {
#if TAG_MAJOR_VERSION == 34
        case SP_HIGH_ELF:
        case SP_SLUDGE_ELF:
            hair = TILEP_HAIR_ELF_YELLOW;
            break;
#endif
        case SP_DEEP_ELF:
            hair = TILEP_HAIR_ELF_WHITE;
            break;
        case SP_TROLL:
            hair = TILEP_HAIR_TROLL;
            break;
        case SP_BASE_DRACONIAN:
        case SP_RED_DRACONIAN:
        case SP_WHITE_DRACONIAN:
        case SP_GREEN_DRACONIAN:
        case SP_YELLOW_DRACONIAN:
        case SP_GREY_DRACONIAN:
        case SP_BLACK_DRACONIAN:
        case SP_PURPLE_DRACONIAN:
        case SP_PALE_DRACONIAN:
        {
            tilep_draconian_init(sp, level, &result, &wing);
            hair   = 0;
            break;
        }
        case SP_MERFOLK:
            result = you.fishtail ? TILEP_BASE_MERFOLK_WATER
                                  : TILEP_BASE_MERFOLK;
            hair = TILEP_HAIR_GREEN;
            break;
        case SP_NAGA:
        case SP_DJINNI:
            hair = TILEP_HAIR_PART2_RED;
            break;
        case SP_VAMPIRE:
            hair = TILEP_HAIR_ARWEN;
            break;
        case SP_SPRIGGAN:
            hair = 0;
            beard = TILEP_BEARD_MEDIUM_GREEN;
            break;
#if TAG_MAJOR_VERSION == 34
        case SP_HILL_ORC:
#endif
        case SP_MINOTAUR:
        case SP_DEMONSPAWN:
        case SP_GHOUL:
        case SP_KOBOLD:
        case SP_MUMMY:
        case SP_FORMICID:
        case SP_BARACHI:
        case SP_GNOLL:
        case SP_GARGOYLE:
        case SP_VINE_STALKER:
            hair = 0;
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

#if TAG_MAJOR_VERSION == 34
        case JOB_SKALD:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_WHITE3;
            parts[TILEP_PART_LEG]   = TILEP_LEG_SKIRT_OFS;
            parts[TILEP_PART_HELM]  = TILEP_HELM_HELM_IRON;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_GRAY;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_GRAY;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLUE;
            break;
#endif

        case JOB_CHAOS_KNIGHT:
            parts[TILEP_PART_BODY]  = TILEP_BODY_MESH_BLACK;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_SHORT_DARKBROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_CLOWN; // Xom
            break;

#if TAG_MAJOR_VERSION == 34
        case JOB_ABYSSAL_KNIGHT:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHOULDER_PAD;
            parts[TILEP_PART_LEG]   = TILEP_LEG_METAL_GRAY;
            parts[TILEP_PART_HELM]  = TILEP_HELM_FHELM_PLUME;
            break;
#endif

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

        case JOB_BRIGAND:
            parts[TILEP_PART_HELM]  = TILEP_HELM_MASK_NINJA_BLACK;
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_BLACK3;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BLACK;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_SWORD_THIEF;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_BLACK;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLACK;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN2;
            break;

        case JOB_HEDGE_WIZARD:
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

        case JOB_HEALER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_WHITE;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_WHITE;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_DAGGER;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_FHELM_HEALER;
            break;
#endif

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

        case JOB_ALCHEMIST:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_GREEN;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_GANDALF;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_GREEN_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_SHAPESHIFTER:
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

        case JOB_HEXSLINGER:
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
            parts[TILEP_PART_HAND2] = TILEP_HAND2_KITE_SHIELD_ROUND2;
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
            parts[TILEP_PART_BODY]  = TILEP_BODY_LEATHER_ARMOUR;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BLACK;
            break;

        case JOB_DELVER: // stolen from JOB_STALKER
            parts[TILEP_PART_HELM]  = TILEP_HELM_HOOD_GREEN;
            parts[TILEP_PART_BODY]  = TILEP_BODY_LEATHER_JACKET;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_SHORT_GRAY;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_SWORD_THIEF;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_GREEN_DIM;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_WRIST_PURPLE;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_GREEN;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_BROWN2;
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

    if (is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_NAGA)
        || is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_ARMATAUR))
    {
        flag[TILEP_PART_BOOTS] = flag[TILEP_PART_LEG] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_CUT_BOTTOM;
    }
    else if (is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_MERFOLK_WATER)
             || is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_DJINNI))
    {
        flag[TILEP_PART_BOOTS]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_LEG]    = TILEP_FLAG_HIDE;
        flag[TILEP_PART_SHADOW] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]   = TILEP_FLAG_CUT_BOTTOM;
    }
    else if (doll.parts[TILEP_PART_BASE] >= TILEP_BASE_DRACONIAN_FIRST
             && doll.parts[TILEP_PART_BASE] <= TILEP_BASE_DRACONIAN_LAST)
    {
        flag[TILEP_PART_HAIR] = TILEP_FLAG_HIDE;
    }
    else if (is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_FELID))
    {
        flag[TILEP_PART_CLOAK] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BOOTS] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_LEG]   = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_ARM]   = TILEP_FLAG_HIDE;
        if (!is_player_tile(doll.parts[TILEP_PART_HAND1],
                            TILEP_HAND1_BLADEHAND_FE))
        {
            flag[TILEP_PART_HAND1] = TILEP_FLAG_HIDE;
            flag[TILEP_PART_HAND2] = TILEP_FLAG_HIDE;
        }
        if (!is_player_tile(doll.parts[TILEP_PART_HELM], TILEP_HELM_HORNS_CAT))
            flag[TILEP_PART_HELM] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_HAIR]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BEARD] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_SHADOW]= TILEP_FLAG_HIDE;
        flag[TILEP_PART_DRCWING]=TILEP_FLAG_HIDE;
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
    }

    if (doll.parts[TILEP_PART_ARM] == TILEP_ARM_OCTOPODE_SPIKE
        && !is_player_tile(doll.parts[TILEP_PART_BASE], TILEP_BASE_OCTOPODE))
    {
        flag[TILEP_PART_ARM] = TILEP_FLAG_HIDE;
    }
    if (is_player_tile(doll.parts[TILEP_PART_HELM], TILEP_HELM_HORNS_CAT)
        && (!is_player_tile(doll.parts[TILEP_PART_BASE],
                            TILEP_BASE_FELID)
            && (!is_player_tile(doll.parts[TILEP_PART_BASE],
                                TILEP_TRAN_STATUE_FELID)
            // Every felid tile has its own horns.
            || doll.parts[TILEP_PART_BASE] - TILEP_BASE_FELID
               != doll.parts[TILEP_PART_HELM] - TILEP_HELM_HORNS_CAT)))
    {
        flag[TILEP_PART_ARM] = TILEP_FLAG_HIDE;
    }
}

// Take a paperdoll and pass out values indicating how and in what order parts
// should be drawn.
void tilep_fill_order_and_flags(const dolls_data &doll, int (&order)[TILEP_PART_MAX],
                                int (&flags)[TILEP_PART_MAX])
{
    // Ordered from back to front.
    static int p_order[TILEP_PART_MAX] =
    {
        // background
        TILEP_PART_SHADOW,
        TILEP_PART_HALO,
        TILEP_PART_ENCH,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        // player
        TILEP_PART_BASE,
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_HELM,
        TILEP_PART_HAND1,
        TILEP_PART_HAND1_MIRROR,
        TILEP_PART_HAND2,
    };

    // Copy default order
    for (int i = 0; i < TILEP_PART_MAX; ++i)
        order[i] = p_order[i];

    // For skirts, boots go under the leg armour. For pants, they go over.
    if (doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        order[7] = TILEP_PART_BOOTS;
        order[6] = TILEP_PART_LEG;
    }

    // Draw scarves above other clothing.
    if (doll.parts[TILEP_PART_CLOAK] >= TILEP_CLOAK_SCARF_FIRST_NORM)
    {
        order[4] = order[5];
        order[5] = order[6];
        order[6] = order[7];
        order[7] = order[8];
        order[8] = order[9];
        order[9] = TILEP_PART_CLOAK;
    }

    tilep_calc_flags(doll, flags);
    reveal_bardings(doll.parts, flags);
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
//    TILEP_PART_DRCHEAD, // XXX: does this break save compat..?
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
        else if (idx > _part_count(p))
            doll.parts[p] = _part_start(p);
        else
        {
            const tileidx_t idx2 = _part_start(p) + idx - 1;
            if (get_tile_texture(idx2) != TEX_PLAYER)
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
                idx = doll.parts[p] - _part_start(p) + 1;
                if (idx > _part_count(p))
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

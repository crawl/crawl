/*
 *  File:       tilepick.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE
#include <stdio.h>
#include "areas.h"
#include "artefact.h"
#include "decks.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "map_knowledge.h"
#include "fprop.h"
#include "externs.h"
#include "options.h"
#include "food.h"
#include "itemname.h"
#include "items.h"
#include "itemprop.h"
#include "kills.h"
#include "libutil.h"
#include "macro.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "player.h"
#include "shopping.h"
#include "showsymb.h"
#include "spells3.h" // for the halo
#include "stuff.h"
#include "terrain.h"
#include "tiles.h"
#include "tilemcache.h"
#include "tiledef-dngn.h"
#include "tiledef-player.h"
#include "tiledef-gui.h"
#include "tiledef-unrand.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "viewgeom.h"

void TileNewLevel(bool first_time)
{
    tiles.clear_minimap();

    for (unsigned int x = 0; x < GXM; x++)
        for (unsigned int y = 0; y < GYM; y++)
            tiles.update_minimap(x, y);

    if (first_time)
        tile_init_flavour();

    if (!player_in_mappable_area() || first_time)
    {
        for (unsigned int x = 0; x < GXM; x++)
            for (unsigned int y = 0; y < GYM; y++)
            {
                env.tile_bk_fg[x][y] = 0;
                env.tile_bk_bg[x][y] = TILE_DNGN_UNSEEN;
            }
    }

    // Fix up stair markers.  The travel information isn't hooked up
    // until after we change levels.  So, look through all of the stairs
    // on this level and check if they still need the stair flag.
    for (unsigned int x = 0; x < GXM; x++)
        for (unsigned int y = 0; y < GYM; y++)
        {
            unsigned int tile = env.tile_bk_bg[x][y];
            if (!(tile & TILE_FLAG_NEW_STAIR))
                continue;
            if (!is_unknown_stair(coord_def(x,y)))
                env.tile_bk_bg[x][y] &= ~TILE_FLAG_NEW_STAIR;
        }
}

/**** tile index routines ****/

int tile_unseen_flag(const coord_def& gc)
{
    if (!map_bounds(gc))
        return TILE_FLAG_UNSEEN;
    else if (is_terrain_known(gc)
                && !is_terrain_seen(gc)
             || is_map_knowledge_detected_item(gc)
             || is_map_knowledge_detected_mons(gc))
    {
        return TILE_FLAG_MM_UNSEEN;
    }
    else
        return TILE_FLAG_UNSEEN;
}

// Special case for *taurs which have a different tile
// for when they have a bow.
static int _bow_offset(const monsters *mon)
{
    int mon_wep = mon->inv[MSLOT_WEAPON];
    if (mon_wep == NON_ITEM)
        return (1);

    switch (mitm[mon_wep].sub_type)
    {
    case WPN_BOW:
    case WPN_LONGBOW:
    case WPN_CROSSBOW:
        return (0);
    default:
        return (1);
    }
}

static int _get_random_monster_tile(const monsters *mon, const int base_tile)
{
    if (!mon->props.exists("tile_num"))
        return (base_tile);

    const int variants = tile_player_count(base_tile);
    return base_tile + (mon->props["tile_num"].get_short() % variants);
}

static bool _is_skeleton(const int z_type)
{
    return (z_type == MONS_SKELETON_SMALL || z_type == MONS_SKELETON_LARGE);
}

static int _tileidx_monster_zombified(const monsters *mon)
{
    const int z_type = mon->type;

    // TODO: Add tiles and code for these as well.
    switch (z_type)
    {
    case MONS_SIMULACRUM_SMALL: return TILEP_MONS_SIMULACRUM_SMALL;
    case MONS_SIMULACRUM_LARGE: return TILEP_MONS_SIMULACRUM_LARGE;
    }

    const int subtype = (int) mons_zombie_base(mon);
    const int z_size = mons_zombie_size(subtype);

    int z_tile;
    switch (get_mon_shape(mon))
    {
    case MON_SHAPE_HUMANOID:
    case MON_SHAPE_HUMANOID_WINGED:
    case MON_SHAPE_HUMANOID_TAILED:
    case MON_SHAPE_HUMANOID_WINGED_TAILED:
        if (z_type == MONS_SKELETON_SMALL)
            return TILEP_MONS_SKELETON_SMALL;
        else if (z_type == MONS_SKELETON_LARGE)
            return TILEP_MONS_SKELETON_LARGE;

        z_tile = (z_size == Z_SMALL ? TILEP_MONS_ZOMBIE_SMALL
                                    : TILEP_MONS_ZOMBIE_LARGE);
        break;
    case MON_SHAPE_CENTAUR:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_CENTAUR;

        z_tile = TILEP_MONS_ZOMBIE_CENTAUR;
        break;
    case MON_SHAPE_NAGA:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_NAGA;

        z_tile = TILEP_MONS_ZOMBIE_NAGA;
        break;
    case MON_SHAPE_QUADRUPED_WINGED:
        if (mons_genus(subtype) == MONS_DRAGON)
        {
            if (_is_skeleton(z_type))
                return TILEP_MONS_SKELETON_DRAGON;

            z_tile = TILEP_MONS_ZOMBIE_DRAGON;
            break;
        }
        // else fall-through
    case MON_SHAPE_QUADRUPED:
        if (mons_genus(subtype) == MONS_HYDRA)
        {
            if (_is_skeleton(z_type))
            {
                return TILEP_MONS_SKELETON_HYDRA
                       + std::min((int)mon->number, 5) - 1;
            }

            z_tile = TILEP_MONS_ZOMBIE_HYDRA
                     + std::min((int)mon->number, 5) - 1;
            break;
        }
        // else fall-through
    case MON_SHAPE_QUADRUPED_TAILLESS:
        if (z_type == MONS_SKELETON_SMALL)
            return TILEP_MONS_SKELETON_QUADRUPED_SMALL;
        else if (z_type == MONS_SKELETON_LARGE)
            return TILEP_MONS_SKELETON_QUADRUPED_LARGE;

        z_tile = (z_size == Z_SMALL ? TILEP_MONS_ZOMBIE_QUADRUPED_SMALL
                                    : TILEP_MONS_ZOMBIE_QUADRUPED_LARGE);
        break;
    case MON_SHAPE_BAT:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_BAT;

        z_tile = TILEP_MONS_ZOMBIE_BAT;
        break;
    case MON_SHAPE_SNAKE:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_SNAKE;

        z_tile = TILEP_MONS_ZOMBIE_SNAKE;
        break;
    case MON_SHAPE_FISH:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_FISH;

        z_tile = TILEP_MONS_ZOMBIE_FISH;
        break;
    case MON_SHAPE_INSECT:
        z_tile = TILEP_MONS_ZOMBIE_BEETLE;
        break;
    case MON_SHAPE_INSECT_WINGED:
        z_tile = TILEP_MONS_ZOMBIE_BEE;
        break;
    case MON_SHAPE_ARACHNID:
        z_tile = TILEP_MONS_ZOMBIE_SPIDER;
        break;
    default:
        z_tile = TILEP_ERROR;
    }

    if (z_type == MONS_SPECTRAL_THING)
        z_tile += (TILEP_MONS_SPECTRAL_SMALL - TILEP_MONS_ZOMBIE_SMALL);

    return (z_tile);
}

int tileidx_monster_base(const monsters *mon, bool detected)
{
    bool in_water = feat_is_water(grd(mon->pos()));
    const bool misled = (!crawl_state.game_is_arena() && you.misled());

    int type = mon->type;
    if (misled)
        type = mon->get_mislead_type();

    // Show only base class for detected monsters.
    if (detected)
        type = mons_detected_base(mon->type);
    else if (!misled && mons_is_zombified(mon))
        return _tileidx_monster_zombified(mon);

    if (mon->props.exists("monster_tile"))
        return int(mon->props["monster_tile"].get_short());

    switch (type)
    {
    // program bug
    case MONS_PROGRAM_BUG:
        return TILEP_MONS_PROGRAM_BUG;

    // insects ('a')
    case MONS_GIANT_COCKROACH:
        return TILEP_MONS_GIANT_COCKROACH;
    case MONS_GIANT_ANT:
        return TILEP_MONS_GIANT_ANT;
    case MONS_SOLDIER_ANT:
        return TILEP_MONS_SOLDIER_ANT;
    case MONS_QUEEN_ANT:
        return TILEP_MONS_QUEEN_ANT;

    // batty monsters ('b')
    case MONS_GIANT_BAT:
        return TILEP_MONS_GIANT_BAT;
    case MONS_BUTTERFLY:
        return TILEP_MONS_BUTTERFLY + ((mon->colour) % 7);

    // centaurs ('c')
    case MONS_CENTAUR:
        return TILEP_MONS_CENTAUR + _bow_offset(mon);
    case MONS_CENTAUR_WARRIOR:
        return TILEP_MONS_CENTAUR_WARRIOR + _bow_offset(mon);
    case MONS_YAKTAUR:
        return TILEP_MONS_YAKTAUR + _bow_offset(mon);
    case MONS_YAKTAUR_CAPTAIN:
        return TILEP_MONS_YAKTAUR_CAPTAIN + _bow_offset(mon);

    // draconians ('d'):
    case MONS_DRACONIAN:
        return TILEP_DRACO_BASE;

    // elves ('e')
    case MONS_ELF:
        return TILEP_MONS_ELF;
    case MONS_DEEP_ELF_SOLDIER:
        return TILEP_MONS_DEEP_ELF_SOLDIER;
    case MONS_DEEP_ELF_FIGHTER:
        return TILEP_MONS_DEEP_ELF_FIGHTER;
    case MONS_DEEP_ELF_KNIGHT:
        return TILEP_MONS_DEEP_ELF_KNIGHT;
    case MONS_DEEP_ELF_MAGE:
        return TILEP_MONS_DEEP_ELF_MAGE;
    case MONS_DEEP_ELF_SUMMONER:
        return TILEP_MONS_DEEP_ELF_SUMMONER;
    case MONS_DEEP_ELF_CONJURER:
        return TILEP_MONS_DEEP_ELF_CONJURER;
    case MONS_DEEP_ELF_PRIEST:
        return TILEP_MONS_DEEP_ELF_PRIEST;
    case MONS_DEEP_ELF_HIGH_PRIEST:
        return TILEP_MONS_DEEP_ELF_HIGH_PRIEST;
    case MONS_DEEP_ELF_DEMONOLOGIST:
        return TILEP_MONS_DEEP_ELF_DEMONOLOGIST;
    case MONS_DEEP_ELF_ANNIHILATOR:
        return TILEP_MONS_DEEP_ELF_ANNIHILATOR;
    case MONS_DEEP_ELF_SORCERER:
        return TILEP_MONS_DEEP_ELF_SORCERER;
    case MONS_DEEP_ELF_DEATH_MAGE:
        return TILEP_MONS_DEEP_ELF_DEATH_MAGE;
    case MONS_DEEP_ELF_BLADEMASTER:
        return TILEP_MONS_DEEP_ELF_BLADEMASTER;
    case MONS_DEEP_ELF_MASTER_ARCHER:
        return TILEP_MONS_DEEP_ELF_MASTER_ARCHER;

    // fungi ('f')
    case MONS_BALLISTOMYCETE:
        if (!detected && mon->has_ench(ENCH_SPORE_PRODUCTION))
            return TILEP_MONS_BALLISTOMYCETE_ACTIVE;
        return TILEP_MONS_BALLISTOMYCETE_INACTIVE;
    case MONS_TOADSTOOL:
        return _get_random_monster_tile(mon, TILEP_MONS_TOADSTOOL);
    case MONS_FUNGUS:
        return TILEP_MONS_FUNGUS;
    case MONS_WANDERING_MUSHROOM:
        return TILEP_MONS_WANDERING_MUSHROOM;

    // goblins ('g')
    case MONS_GOBLIN:
        return TILEP_MONS_GOBLIN;
    case MONS_HOBGOBLIN:
        return TILEP_MONS_HOBGOBLIN;
    case MONS_GNOLL:
        return TILEP_MONS_GNOLL;
    case MONS_BOGGART:
        return TILEP_MONS_BOGGART;

    // hounds and hogs ('h')
    case MONS_JACKAL:
        return TILEP_MONS_JACKAL;
    case MONS_HOUND:
        return TILEP_MONS_HOUND;
    case MONS_WARG:
        return TILEP_MONS_WARG;
    case MONS_WOLF:
        return TILEP_MONS_WOLF;
    case MONS_WAR_DOG:
        return TILEP_MONS_WAR_DOG;
    case MONS_HOG:
        return TILEP_MONS_HOG;
    case MONS_HELL_HOUND:
        return TILEP_MONS_HELL_HOUND;
    case MONS_HELL_HOG:
        return TILEP_MONS_HELL_HOG;

    // slugs ('j')
    case MONS_ELEPHANT_SLUG:
        return TILEP_MONS_ELEPHANT_SLUG;
    case MONS_GIANT_SLUG:
        return TILEP_MONS_GIANT_SLUG;
    case MONS_GIANT_SNAIL:
        return TILEP_MONS_GIANT_SNAIL;

    // killer bees ('k')
    case MONS_KILLER_BEE:
        return TILEP_MONS_KILLER_BEE;
    case MONS_BUMBLEBEE:
        return TILEP_MONS_BUMBLEBEE;
    case MONS_QUEEN_BEE:
        return TILEP_MONS_QUEEN_BEE;

    // lizards ('l')
    case MONS_GIANT_NEWT:
        return TILEP_MONS_GIANT_NEWT;
    case MONS_GIANT_GECKO:
        return TILEP_MONS_GIANT_GECKO;
    case MONS_IGUANA:
        return TILEP_MONS_IGUANA;
    case MONS_GILA_MONSTER:
        return TILEP_MONS_GILA_MONSTER;
    case MONS_KOMODO_DRAGON:
        return TILEP_MONS_KOMODO_DRAGON;

    // drakes (also 'l', but dragon type)
    case MONS_SWAMP_DRAKE:
        return TILEP_MONS_SWAMP_DRAKE;
    case MONS_FIRE_DRAKE:
        return TILEP_MONS_FIRE_DRAKE;
    case MONS_LINDWURM:
        return TILEP_MONS_LINDWURM;
    case MONS_DEATH_DRAKE:
        return TILEP_MONS_DEATH_DRAKE;

    // merfolk ('m')
    case MONS_MERFOLK:
        if (in_water)
            return TILEP_MONS_MERFOLK_WATER;
        else
            return TILEP_MONS_MERFOLK;
    case MONS_MERFOLK_IMPALER:
        if (in_water)
            return TILEP_MONS_MERFOLK_IMPALER_WATER;
        else
            return TILEP_MONS_MERFOLK_IMPALER;
    case MONS_MERFOLK_AQUAMANCER:
        if (in_water)
            return TILEP_MONS_MERFOLK_AQUAMANCER_WATER;
        else
            return TILEP_MONS_MERFOLK_AQUAMANCER;
    case MONS_MERFOLK_JAVELINEER:
        if (in_water)
            return TILEP_MONS_MERFOLK_JAVELINEER_WATER;
        else
            return TILEP_MONS_MERFOLK_JAVELINEER;
    case MONS_MERMAID:
        if (in_water)
            return TILEP_MONS_MERMAID_WATER;
        else
            return TILEP_MONS_MERMAID;
    case MONS_SIREN:
        if (in_water)
            return TILEP_MONS_SIREN_WATER;
        else
            return TILEP_MONS_SIREN;

    // rotting monsters ('n')
    case MONS_NECROPHAGE:
        return TILEP_MONS_NECROPHAGE;
    case MONS_GHOUL:
        return TILEP_MONS_GHOUL;
    case MONS_ROTTING_HULK:
        return TILEP_MONS_ROTTING_HULK;

    // orcs ('o')
    case MONS_ORC:
        return TILEP_MONS_ORC;
    case MONS_ORC_WIZARD:
        return TILEP_MONS_ORC_WIZARD;
    case MONS_ORC_PRIEST:
        return TILEP_MONS_ORC_PRIEST;
    case MONS_ORC_WARRIOR:
        return TILEP_MONS_ORC_WARRIOR;
    case MONS_ORC_KNIGHT:
        return TILEP_MONS_ORC_KNIGHT;
    case MONS_ORC_WARLORD:
        return TILEP_MONS_ORC_WARLORD;
    case MONS_ORC_SORCERER:
        return TILEP_MONS_ORC_SORCERER;
    case MONS_ORC_HIGH_PRIEST:
        return TILEP_MONS_ORC_HIGH_PRIEST;

    // phantoms and ghosts ('p')
    case MONS_PHANTOM:
        return TILEP_MONS_PHANTOM;
    case MONS_HUNGRY_GHOST:
        return TILEP_MONS_HUNGRY_GHOST;
    case MONS_FLAYED_GHOST:
        return TILEP_MONS_FLAYED_GHOST;
    case MONS_PLAYER_GHOST:
    case MONS_PLAYER_ILLUSION:
        return TILEP_MONS_PLAYER_GHOST;
    case MONS_INSUBSTANTIAL_WISP:
        return TILEP_MONS_INSUBSTANTIAL_WISP;

    // rodents ('r')
    case MONS_RAT:
        return TILEP_MONS_RAT;
    case MONS_QUOKKA:
        return TILEP_MONS_QUOKKA;
    case MONS_GREY_RAT:
        return TILEP_MONS_GREY_RAT;
    case MONS_GREEN_RAT:
        return TILEP_MONS_GREEN_RAT;
    case MONS_ORANGE_RAT:
        return TILEP_MONS_ORANGE_RAT;

    // spiders and insects ('s')
    case MONS_GIANT_MITE:
        return TILEP_MONS_GIANT_MITE;
    case MONS_GIANT_CENTIPEDE:
        return TILEP_MONS_GIANT_CENTIPEDE;
    case MONS_SCORPION:
        return TILEP_MONS_SCORPION;
    case MONS_WOLF_SPIDER:
        return TILEP_MONS_WOLF_SPIDER;
    case MONS_TRAPDOOR_SPIDER:
        return TILEP_MONS_TRAPDOOR_SPIDER;
    case MONS_REDBACK:
        return TILEP_MONS_REDBACK;
    case MONS_DEMONIC_CRAWLER:
        return TILEP_MONS_DEMONIC_CRAWLER;

    // turtles and crocodiles ('t')
    case MONS_CROCODILE:
        return TILEP_MONS_CROCODILE;
    case MONS_BABY_ALLIGATOR:
        return TILEP_MONS_BABY_ALLIGATOR;
    case MONS_ALLIGATOR:
        return TILEP_MONS_ALLIGATOR;
    case MONS_SNAPPING_TURTLE:
        return TILEP_MONS_SNAPPING_TURTLE;
    case MONS_ALLIGATOR_SNAPPING_TURTLE:
        return TILEP_MONS_ALLIGATOR_SNAPPING_TURTLE;

    // ugly things ('u')
    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        const int ugly_tile = (type == MONS_VERY_UGLY_THING) ?
            TILEP_MONS_VERY_UGLY_THING : TILEP_MONS_UGLY_THING;
        int colour_offset = ugly_thing_colour_offset(mon->colour);

        if (detected || colour_offset == -1)
            colour_offset = 0;

        return (ugly_tile + colour_offset);
    }

    // vortices ('v')
    case MONS_FIRE_VORTEX:
        return TILEP_MONS_FIRE_VORTEX;
    case MONS_SPATIAL_VORTEX:
        return TILEP_MONS_SPATIAL_VORTEX;

    // elementals (different symbols)
    case MONS_AIR_ELEMENTAL:
        return TILEP_MONS_AIR_ELEMENTAL;
    case MONS_EARTH_ELEMENTAL:
        return TILEP_MONS_EARTH_ELEMENTAL;
    case MONS_FIRE_ELEMENTAL:
        return TILEP_MONS_FIRE_ELEMENTAL;
    case MONS_WATER_ELEMENTAL:
        return TILEP_MONS_WATER_ELEMENTAL;

    // worms and larvae ('w')
    case MONS_KILLER_BEE_LARVA:
        return TILEP_MONS_KILLER_BEE_LARVA;
    case MONS_WORM:
        return TILEP_MONS_WORM;
    case MONS_ANT_LARVA:
        return TILEP_MONS_ANT_LARVA;
    case MONS_BRAIN_WORM:
        return TILEP_MONS_BRAIN_WORM;
    case MONS_SWAMP_WORM:
        return TILEP_MONS_SWAMP_WORM;
    case MONS_GIANT_LEECH:
        return TILEP_MONS_GIANT_LEECH;
    case MONS_SPINY_WORM:
        return TILEP_MONS_SPINY_WORM;

    // small abominations ('x')
    case MONS_UNSEEN_HORROR:
        return TILEP_MONS_UNSEEN_HORROR;
    case MONS_ABOMINATION_SMALL:
        return TILEP_MONS_ABOMINATION_SMALL;

    // flying insects ('y')
    case MONS_YELLOW_WASP:
        return TILEP_MONS_YELLOW_WASP;
    case MONS_GIANT_MOSQUITO:
        return TILEP_MONS_GIANT_MOSQUITO;
    case MONS_GIANT_BLOWFLY:
        return TILEP_MONS_GIANT_BLOWFLY;
    case MONS_RED_WASP:
        return TILEP_MONS_RED_WASP;
    case MONS_MOTH_OF_WRATH:
        return TILEP_MONS_MOTH_OF_WRATH;

    // small zombies etc. ('z')
    case MONS_ZOMBIE_SMALL:
        return TILEP_MONS_ZOMBIE_SMALL;
    case MONS_SIMULACRUM_SMALL:
        return TILEP_MONS_SIMULACRUM_SMALL;
    case MONS_SKELETON_SMALL:
        return TILEP_MONS_SKELETON_SMALL;
    case MONS_SKELETAL_WARRIOR:
        return TILEP_MONS_SKELETAL_WARRIOR;
    case MONS_FLYING_SKULL:
        return TILEP_MONS_FLYING_SKULL;
    case MONS_FLAMING_CORPSE:
        return TILEP_MONS_FLAMING_CORPSE;
    case MONS_CURSE_SKULL:
        return TILEP_MONS_CURSE_SKULL;
    case MONS_CURSE_TOE:
        return TILEP_MONS_CURSE_TOE;

    // angelic beings ('A')
    case MONS_ANGEL:
        return TILEP_MONS_ANGEL;
    case MONS_DAEVA:
        return TILEP_MONS_DAEVA;

    // beetles ('B')
    case MONS_GIANT_BEETLE:
        return TILEP_MONS_GIANT_BEETLE;
    case MONS_BOULDER_BEETLE:
        return TILEP_MONS_BOULDER_BEETLE;
    case MONS_BORING_BEETLE:
        return TILEP_MONS_BORING_BEETLE;

    // cyclops and giants ('C')
    case MONS_HILL_GIANT:
        return TILEP_MONS_HILL_GIANT;
    case MONS_ETTIN:
        return TILEP_MONS_ETTIN;
    case MONS_CYCLOPS:
        return TILEP_MONS_CYCLOPS;
    case MONS_FIRE_GIANT:
        return TILEP_MONS_FIRE_GIANT;
    case MONS_FROST_GIANT:
        return TILEP_MONS_FROST_GIANT;
    case MONS_STONE_GIANT:
        return TILEP_MONS_STONE_GIANT;
    case MONS_TITAN:
        return TILEP_MONS_TITAN;

    // dragons ('D')
    case MONS_WYVERN:
        return TILEP_MONS_WYVERN;
    case MONS_DRAGON:
        return TILEP_MONS_DRAGON;
    case MONS_HYDRA:
        // Number of heads
        return TILEP_MONS_HYDRA + std::min((int)mon->number, 7) - 1;
    case MONS_ICE_DRAGON:
        return TILEP_MONS_ICE_DRAGON;
    case MONS_STEAM_DRAGON:
        return TILEP_MONS_STEAM_DRAGON;
    case MONS_SWAMP_DRAGON:
        return TILEP_MONS_SWAMP_DRAGON;
    case MONS_MOTTLED_DRAGON:
        return TILEP_MONS_MOTTLED_DRAGON;
    case MONS_QUICKSILVER_DRAGON:
        return TILEP_MONS_QUICKSILVER_DRAGON;
    case MONS_IRON_DRAGON:
        return TILEP_MONS_IRON_DRAGON;
    case MONS_STORM_DRAGON:
        return TILEP_MONS_STORM_DRAGON;
    case MONS_GOLDEN_DRAGON:
        return TILEP_MONS_GOLDEN_DRAGON;
    case MONS_SHADOW_DRAGON:
        return TILEP_MONS_SHADOW_DRAGON;
    case MONS_BONE_DRAGON:
        return TILEP_MONS_BONE_DRAGON;
    case MONS_SERPENT_OF_HELL:
        return TILEP_MONS_SERPENT_OF_HELL;

    // efreet ('E')
    case MONS_EFREET:
        return TILEP_MONS_EFREET;

    // frogs ('F')
    case MONS_GIANT_FROG:
        return TILEP_MONS_GIANT_FROG;
    case MONS_GIANT_TOAD:
        return TILEP_MONS_GIANT_TOAD;
    case MONS_SPINY_FROG:
        return TILEP_MONS_SPINY_FROG;
    case MONS_BLINK_FROG:
        return TILEP_MONS_BLINK_FROG;

    // eyes and spores ('G')
    case MONS_GIANT_SPORE:
        return TILEP_MONS_GIANT_SPORE;
    case MONS_GIANT_EYEBALL:
        return TILEP_MONS_GIANT_EYEBALL;
    case MONS_EYE_OF_DRAINING:
        return TILEP_MONS_EYE_OF_DRAINING;
    case MONS_GIANT_ORANGE_BRAIN:
        return TILEP_MONS_GIANT_ORANGE_BRAIN;
    case MONS_GREAT_ORB_OF_EYES:
        return TILEP_MONS_GREAT_ORB_OF_EYES;
    case MONS_SHINING_EYE:
        return TILEP_MONS_SHINING_EYE;
    case MONS_EYE_OF_DEVASTATION:
        return TILEP_MONS_EYE_OF_DEVASTATION;
    case MONS_GOLDEN_EYE:
        return TILEP_MONS_GOLDEN_EYE;

    // hybrids ('H')
    case MONS_HIPPOGRIFF:
        return TILEP_MONS_HIPPOGRIFF;
    case MONS_MANTICORE:
        return TILEP_MONS_MANTICORE;
    case MONS_GRIFFON:
        return TILEP_MONS_GRIFFON;
    case MONS_SPHINX:
        return TILEP_MONS_SPHINX;
    case MONS_HARPY:
        return TILEP_MONS_HARPY;
    case MONS_MINOTAUR:
        return TILEP_MONS_MINOTAUR;

    // ice beast ('I')
    case MONS_ICE_BEAST:
        return TILEP_MONS_ICE_BEAST;

    // jellies ('J')
    case MONS_OOZE:
        return TILEP_MONS_OOZE;
    case MONS_JELLY:
        return TILEP_MONS_JELLY;
    case MONS_SLIME_CREATURE:
        ASSERT(mon->number <= 5);
        return TILEP_MONS_SLIME_CREATURE + (mon->number ? mon->number - 1 : 0);
    case MONS_PULSATING_LUMP:
        return TILEP_MONS_PULSATING_LUMP;
    case MONS_GIANT_AMOEBA:
        return TILEP_MONS_GIANT_AMOEBA;
    case MONS_BROWN_OOZE:
        return TILEP_MONS_BROWN_OOZE;
    case MONS_AZURE_JELLY:
        return TILEP_MONS_AZURE_JELLY;
    case MONS_DEATH_OOZE:
        return TILEP_MONS_DEATH_OOZE;
    case MONS_ACID_BLOB:
        return TILEP_MONS_ACID_BLOB;
    case MONS_ROYAL_JELLY:
        return TILEP_MONS_ROYAL_JELLY;

    // kobolds ('K')
    case MONS_KOBOLD:
        return TILEP_MONS_KOBOLD;
    case MONS_BIG_KOBOLD:
        return TILEP_MONS_BIG_KOBOLD;
    case MONS_KOBOLD_DEMONOLOGIST:
        return TILEP_MONS_KOBOLD_DEMONOLOGIST;

    // liches ('L')
    case MONS_LICH:
        return TILEP_MONS_LICH;
    case MONS_ANCIENT_LICH:
        return TILEP_MONS_ANCIENT_LICH;

    // mummies ('M')
    case MONS_MUMMY:
        return TILEP_MONS_MUMMY;
    case MONS_GUARDIAN_MUMMY:
        return TILEP_MONS_GUARDIAN_MUMMY;
    case MONS_GREATER_MUMMY:
        return TILEP_MONS_GREATER_MUMMY;
    case MONS_MUMMY_PRIEST:
        return TILEP_MONS_MUMMY_PRIEST;

    // nagas ('N')
    case MONS_NAGA:
        return TILEP_MONS_NAGA;
    case MONS_GUARDIAN_SERPENT:
        return TILEP_MONS_GUARDIAN_SERPENT;
    case MONS_NAGA_MAGE:
        return TILEP_MONS_NAGA_MAGE;
    case MONS_NAGA_WARRIOR:
        return TILEP_MONS_NAGA_WARRIOR;
    case MONS_GREATER_NAGA:
        return TILEP_MONS_GREATER_NAGA;

    // ogres ('O')
    case MONS_OGRE:
        return TILEP_MONS_OGRE;
    case MONS_TWO_HEADED_OGRE:
        return TILEP_MONS_TWO_HEADED_OGRE;
    case MONS_OGRE_MAGE:
        return TILEP_MONS_OGRE_MAGE;

    // plants ('P')
    case MONS_PLANT:
        return TILEP_MONS_PLANT;
    case MONS_BUSH:
        if (cloud_type_at(mon->pos()) == CLOUD_FIRE)
            return TILEP_MONS_BUSH_BURNING;
        return TILEP_MONS_BUSH;
    case MONS_OKLOB_PLANT:
        return TILEP_MONS_OKLOB_PLANT;

    // rakshasa ('R')
    case MONS_RAKSHASA:
        return TILEP_MONS_RAKSHASA;
    case MONS_RAKSHASA_FAKE:
        return TILEP_MONS_RAKSHASA_FAKE;

    // snakes ('S')
    case MONS_SMALL_SNAKE:
        return TILEP_MONS_SMALL_SNAKE;
    case MONS_SNAKE:
        return TILEP_MONS_SNAKE;
    case MONS_WATER_MOCCASIN:
        return TILEP_MONS_WATER_MOCCASIN;
    case MONS_BLACK_MAMBA:
        return TILEP_MONS_BLACK_MAMBA;
    case MONS_VIPER:
        return TILEP_MONS_VIPER;
    case MONS_ANACONDA:
        return TILEP_MONS_ANACONDA;
    case MONS_SEA_SNAKE:
        return TILEP_MONS_SEA_SNAKE;

    // trolls ('T')
    case MONS_TROLL:
        return TILEP_MONS_TROLL;
    case MONS_ROCK_TROLL:
        return TILEP_MONS_ROCK_TROLL;
    case MONS_IRON_TROLL:
        return TILEP_MONS_IRON_TROLL;
    case MONS_DEEP_TROLL:
        return TILEP_MONS_DEEP_TROLL;

    // bears ('U')
    case MONS_BEAR:
        return TILEP_MONS_BEAR;
    case MONS_GRIZZLY_BEAR:
        return TILEP_MONS_GRIZZLY_BEAR;
    case MONS_POLAR_BEAR:
        return TILEP_MONS_POLAR_BEAR;
    case MONS_BLACK_BEAR:
        return TILEP_MONS_BLACK_BEAR;

    // vampires ('V')
    case MONS_VAMPIRE:
        return TILEP_MONS_VAMPIRE;
    case MONS_VAMPIRE_KNIGHT:
        return TILEP_MONS_VAMPIRE_KNIGHT;
    case MONS_VAMPIRE_MAGE:
        return TILEP_MONS_VAMPIRE_MAGE;

    // wraiths ('W')
    case MONS_WIGHT:
        return TILEP_MONS_WIGHT;
    case MONS_WRAITH:
        return TILEP_MONS_WRAITH;
    case MONS_SHADOW_WRAITH:
        return TILEP_MONS_SHADOW_WRAITH;
    case MONS_FREEZING_WRAITH:
        return TILEP_MONS_FREEZING_WRAITH;
    case MONS_SPECTRAL_WARRIOR:
        return TILEP_MONS_SPECTRAL_WARRIOR;
    case MONS_SPECTRAL_THING:
        return TILEP_MONS_SPECTRAL_LARGE;

    // large abominations ('X')
    case MONS_ABOMINATION_LARGE:
        return TILEP_MONS_ABOMINATION_LARGE + ((mon->colour) % 7);
    case MONS_TENTACLED_MONSTROSITY:
        return TILEP_MONS_TENTACLED_MONSTROSITY;
    case MONS_ORB_GUARDIAN:
        return TILEP_MONS_ORB_GUARDIAN;
    case MONS_TEST_SPAWNER:
        return TILEP_MONS_TEST_SPAWNER;

    // yaks and sheep ('Y')
    case MONS_SHEEP:
        return TILEP_MONS_SHEEP;
    case MONS_YAK:
        return TILEP_MONS_YAK;
    case MONS_DEATH_YAK:
        return TILEP_MONS_DEATH_YAK;

    // large zombies etc. ('Z')
    case MONS_ZOMBIE_LARGE:
        return TILEP_MONS_ZOMBIE_LARGE;
    case MONS_SKELETON_LARGE:
        return TILEP_MONS_SKELETON_LARGE;
    case MONS_SIMULACRUM_LARGE:
        return TILEP_MONS_SIMULACRUM_LARGE;

    // water monsters
    case MONS_BIG_FISH:
        return TILEP_MONS_BIG_FISH;
    case MONS_GIANT_GOLDFISH:
        return TILEP_MONS_GIANT_GOLDFISH;
    case MONS_ELECTRIC_EEL:
        return TILEP_MONS_ELECTRIC_EEL;
    case MONS_SHARK:
        return TILEP_MONS_SHARK;
    case MONS_JELLYFISH:
        return TILEP_MONS_JELLYFISH;
    case MONS_KRAKEN:
        return TILEP_MONS_KRAKEN_HEAD;
    case MONS_KRAKEN_TENTACLE:
        // Pick a random tentacle tile.
        return TILEP_MONS_KRAKEN_TENTACLE
               + random2(tile_player_count(TILEP_MONS_KRAKEN_TENTACLE));

    // lava monsters
    case MONS_LAVA_WORM:
        return TILEP_MONS_LAVA_WORM;
    case MONS_LAVA_FISH:
        return TILEP_MONS_LAVA_FISH;
    case MONS_LAVA_SNAKE:
        return TILEP_MONS_LAVA_SNAKE;
    case MONS_SALAMANDER:
        return TILEP_MONS_SALAMANDER;

    // monsters moving through rock
    case MONS_ROCK_WORM:
        return TILEP_MONS_ROCK_WORM;

    // humans ('@')
    case MONS_HUMAN:
    case MONS_DWARF:
        return TILEP_MONS_HUMAN;
    case MONS_HELL_KNIGHT:
        return TILEP_MONS_HELL_KNIGHT;
    case MONS_NECROMANCER:
        return TILEP_MONS_NECROMANCER;
    case MONS_WIZARD:
        return TILEP_MONS_WIZARD;
    case MONS_VAULT_GUARD:
        return TILEP_MONS_VAULT_GUARD;
    case MONS_SHAPESHIFTER:
        return TILEP_MONS_SHAPESHIFTER;
    case MONS_GLOWING_SHAPESHIFTER:
        return TILEP_MONS_GLOWING_SHAPESHIFTER;
    case MONS_KILLER_KLOWN:
        return TILEP_MONS_KILLER_KLOWN;
    case MONS_SLAVE:
        return _get_random_monster_tile(mon, TILEP_MONS_SLAVE);

    // mimics
    case MONS_GOLD_MIMIC:
    case MONS_WEAPON_MIMIC:
    case MONS_ARMOUR_MIMIC:
    case MONS_SCROLL_MIMIC:
    case MONS_POTION_MIMIC:
    {
        int ch = tileidx_item(get_mimic_item(mon));
        if (mons_is_known_mimic(mon))
            ch |= TILE_FLAG_ANIM_WEP;
        return (ch);
    }

    case MONS_DANCING_WEAPON:
    {
        // Use item tile.
        item_def item = mitm[mon->inv[MSLOT_WEAPON]];
        return tileidx_item(item) | TILE_FLAG_ANIM_WEP;
    }

    // '5' demons
    case MONS_IMP:
        return TILEP_MONS_IMP;
    case MONS_QUASIT:
        return TILEP_MONS_QUASIT;
    case MONS_WHITE_IMP:
        return TILEP_MONS_WHITE_IMP;
    case MONS_LEMURE:
        return TILEP_MONS_LEMURE;
    case MONS_UFETUBUS:
        return TILEP_MONS_UFETUBUS;
    case MONS_IRON_IMP:
        return TILEP_MONS_IRON_IMP;
    case MONS_MIDGE:
        return TILEP_MONS_MIDGE;
    case MONS_SHADOW_IMP:
        return TILEP_MONS_SHADOW_IMP;

    // '4' demons
    case MONS_RED_DEVIL:
        return TILEP_MONS_RED_DEVIL;
    case MONS_HAIRY_DEVIL:
        return TILEP_MONS_HAIRY_DEVIL;
    case MONS_ROTTING_DEVIL:
        return TILEP_MONS_ROTTING_DEVIL;
    case MONS_SMOKE_DEMON:
        return TILEP_MONS_SMOKE_DEMON;
    case MONS_SIXFIRHY:
        return TILEP_MONS_SIXFIRHY;
    case MONS_HELLWING:
        return TILEP_MONS_HELLWING;

    // '3' demons
    case MONS_HELLION:
        return TILEP_MONS_HELLION;
    case MONS_TORMENTOR:
        return TILEP_MONS_TORMENTOR;
    case MONS_BLUE_DEVIL:
        return TILEP_MONS_BLUE_DEVIL;
    case MONS_IRON_DEVIL:
        return TILEP_MONS_IRON_DEVIL;
    case MONS_NEQOXEC:
        return TILEP_MONS_NEQOXEC;
    case MONS_ORANGE_DEMON:
        return TILEP_MONS_ORANGE_DEMON;
    case MONS_YNOXINUL:
        return TILEP_MONS_YNOXINUL;
    case MONS_SHADOW_DEMON:
        return TILEP_MONS_SHADOW_DEMON;
    case MONS_CHAOS_SPAWN:
        return TILEP_MONS_CHAOS_SPAWN;

    // '2' demon
    case MONS_BEAST:
        return TILEP_MONS_BEAST;
    case MONS_SUN_DEMON:
        return TILEP_MONS_SUN_DEMON;
    case MONS_REAPER:
        return TILEP_MONS_REAPER;
    case MONS_SOUL_EATER:
        return TILEP_MONS_SOUL_EATER;
    case MONS_ICE_DEVIL:
        return TILEP_MONS_ICE_DEVIL;
    case MONS_LOROCYPROCA:
        return TILEP_MONS_LOROCYPROCA;

    // '1' demons
    case MONS_FIEND:
        return TILEP_MONS_FIEND;
    case MONS_ICE_FIEND:
        return TILEP_MONS_ICE_FIEND;
    case MONS_SHADOW_FIEND:
        return TILEP_MONS_SHADOW_FIEND;
    case MONS_PIT_FIEND:
        return TILEP_MONS_PIT_FIEND;
    case MONS_EXECUTIONER:
        return TILEP_MONS_EXECUTIONER;
    case MONS_GREEN_DEATH:
        return TILEP_MONS_GREEN_DEATH;
    case MONS_BLUE_DEATH:
        return TILEP_MONS_BLUE_DEATH;
    case MONS_BALRUG:
        return TILEP_MONS_BALRUG;
    case MONS_CACODEMON:
        return TILEP_MONS_CACODEMON;

    // non-living creatures
    // golems ('8')
    case MONS_CLAY_GOLEM:
        return TILEP_MONS_CLAY_GOLEM;
    case MONS_WOOD_GOLEM:
        return TILEP_MONS_WOOD_GOLEM;
    case MONS_IRON_GOLEM:
        return TILEP_MONS_IRON_GOLEM;
    case MONS_STONE_GOLEM:
        return TILEP_MONS_STONE_GOLEM;
    case MONS_CRYSTAL_GOLEM:
        return TILEP_MONS_CRYSTAL_GOLEM;
    case MONS_TOENAIL_GOLEM:
        return TILEP_MONS_TOENAIL_GOLEM;
    case MONS_ELECTRIC_GOLEM:
        return TILEP_MONS_ELECTRIC_GOLEM;

    // statues (also '8')
    case MONS_ICE_STATUE:
        return TILEP_MONS_ICE_STATUE;
    case MONS_SILVER_STATUE:
        return TILEP_MONS_SILVER_STATUE;
    case MONS_ORANGE_STATUE:
        return TILEP_MONS_ORANGE_STATUE;

    // gargoyles ('9')
    case MONS_GARGOYLE:
        return TILEP_MONS_GARGOYLE;
    case MONS_METAL_GARGOYLE:
        return TILEP_MONS_METAL_GARGOYLE;
    case MONS_MOLTEN_GARGOYLE:
        return TILEP_MONS_MOLTEN_GARGOYLE;

    // major demons ('&')
    case MONS_PANDEMONIUM_DEMON:
        return TILEP_MONS_PANDEMONIUM_DEMON;

    // ball lightning / orb of fire ('*')
    case MONS_BALL_LIGHTNING:
        return TILEP_MONS_BALL_LIGHTNING;
    case MONS_ORB_OF_FIRE:
        return TILEP_MONS_ORB_OF_FIRE;
    case MONS_ORB_OF_DESTRUCTION:
        // Pick a random tile.
        return TILEP_MONS_ORB_OF_DESTRUCTION
               + random2(tile_player_count(TILEP_MONS_ORB_OF_DESTRUCTION));

    // other symbols
    case MONS_VAPOUR:
        return TILEP_MONS_VAPOUR;
    case MONS_SHADOW:
        return TILEP_MONS_SHADOW;
    case MONS_DEATH_COB:
        return TILEP_MONS_DEATH_COB;

    // -------------------------------------
    // non-human uniques, sorted by glyph, then difficulty
    // -------------------------------------

    // centaur ('c')
    case MONS_NESSOS:
        return TILEP_MONS_NESSOS;

    // draconian ('d')
    case MONS_TIAMAT:
        return TILEP_MONS_TIAMAT;

    // elves ('e')
    case MONS_DOWAN:
        return TILEP_MONS_DOWAN;
    case MONS_DUVESSA:
        return TILEP_MONS_DUVESSA;

    // goblins and gnolls ('g')
    case MONS_IJYB:
        return TILEP_MONS_IJYB;
    case MONS_CRAZY_YIUF:
        return TILEP_MONS_CRAZY_YIUF;
    case MONS_GRUM:
        return TILEP_MONS_GRUM;

    // slug ('j')
    case MONS_GASTRONOK:
        return TILEP_MONS_GASTRONOK;

    // merfolk ('m')
    case MONS_ILSUIW:
        if (in_water)
            return TILEP_MONS_ILSUIW_WATER;
        else
            return TILEP_MONS_ILSUIW;

    // orcs ('o')
    case MONS_BLORK_THE_ORC:
        return TILEP_MONS_BLORK_THE_ORC;
    case MONS_URUG:
        return TILEP_MONS_URUG;
    case MONS_NERGALLE:
        return TILEP_MONS_NERGALLE;
    case MONS_SAINT_ROKA:
        return TILEP_MONS_SAINT_ROKA;

    // curse skull ('z')
    case MONS_MURRAY:
        return TILEP_MONS_MURRAY;

    // cyclops and giants ('C')
    case MONS_POLYPHEMUS:
        return TILEP_MONS_POLYPHEMUS;
    case MONS_ANTAEUS:
        return TILEP_MONS_ANTAEUS;

    // dragons and hydras ('D')
    case MONS_LERNAEAN_HYDRA:
        return TILEP_MONS_LERNAEAN_HYDRA;
    case MONS_XTAHUA:
        return TILEP_MONS_XTAHUA;

    // efreet ('E')
    case MONS_AZRAEL:
        return TILEP_MONS_AZRAEL;

    // frog ('F')
    case MONS_PRINCE_RIBBIT:
        return TILEP_MONS_PRINCE_RIBBIT;

    // jelly ('J')
    case MONS_DISSOLUTION:
        return TILEP_MONS_DISSOLUTION;

    // kobolds ('K')
    case MONS_SONJA:
        return TILEP_MONS_SONJA;
    case MONS_PIKEL:
        return TILEP_MONS_PIKEL;

    // lich ('L')
    case MONS_BORIS:
        return TILEP_MONS_BORIS;

    // mummies ('M')
    case MONS_MENKAURE:
        return TILEP_MONS_MENKAURE;
    case MONS_KHUFU:
        return TILEP_MONS_KHUFU;

    // guardian serpent ('N')
    case MONS_AIZUL:
        return TILEP_MONS_AIZUL;

    // ogre ('O')
    case MONS_EROLCHA:
        return TILEP_MONS_EROLCHA;

    // rakshasas ('R')
    case MONS_MARA:
        return TILEP_MONS_MARA;
    case MONS_MARA_FAKE:
        return TILEP_MONS_MARA_FAKE;

    // trolls ('T')
    case MONS_PURGY:
        return TILEP_MONS_PURGY;
    case MONS_SNORG:
        return TILEP_MONS_SNORG;

    // statue ('8')
    case MONS_ROXANNE:
        return TILEP_MONS_ROXANNE;

    // -------------------------------------
    // non-human uniques ('@')
    // -------------------------------------

    case MONS_TERENCE:
        return TILEP_MONS_TERENCE;
    case MONS_JESSICA:
        return TILEP_MONS_JESSICA;
    case MONS_SIGMUND:
        return TILEP_MONS_SIGMUND;
    case MONS_EDMUND:
        return TILEP_MONS_EDMUND;
    case MONS_PSYCHE:
        return TILEP_MONS_PSYCHE;
    case MONS_DONALD:
        return TILEP_MONS_DONALD;
    case MONS_JOSEPH:
        return TILEP_MONS_JOSEPH;
    case MONS_ERICA:
        return TILEP_MONS_ERICA;
    case MONS_JOSEPHINE:
        return TILEP_MONS_JOSEPHINE;
    case MONS_HAROLD:
        return TILEP_MONS_HAROLD;
    case MONS_NORBERT:
        return TILEP_MONS_NORBERT;
    case MONS_JOZEF:
        return TILEP_MONS_JOZEF;
    case MONS_AGNES:
        return TILEP_MONS_AGNES;
    case MONS_MAUD:
        return TILEP_MONS_MAUD;
    case MONS_LOUISE:
        return TILEP_MONS_LOUISE;
    case MONS_FRANCIS:
        return TILEP_MONS_FRANCIS;
    case MONS_FRANCES:
        return TILEP_MONS_FRANCES;
    case MONS_RUPERT:
        return TILEP_MONS_RUPERT;
    case MONS_WAYNE:
        return TILEP_MONS_WAYNE;
    case MONS_DUANE:
        return TILEP_MONS_DUANE;
    case MONS_NORRIS:
        return TILEP_MONS_NORRIS;
    case MONS_FREDERICK:
        return TILEP_MONS_FREDERICK;
    case MONS_MARGERY:
        return TILEP_MONS_MARGERY;
    case MONS_EUSTACHIO:
        return TILEP_MONS_EUSTACHIO;
    case MONS_KIRKE:
        return TILEP_MONS_KIRKE;
    case MONS_NIKOLA:
        return TILEP_MONS_NIKOLA;
    case MONS_MAURICE:
        return TILEP_MONS_MAURICE;

    // unique major demons ('&')
    case MONS_MNOLEG:
        return TILEP_MONS_MNOLEG;
    case MONS_LOM_LOBON:
        return TILEP_MONS_LOM_LOBON;
    case MONS_CEREBOV:
        return TILEP_MONS_CEREBOV;
    case MONS_GLOORX_VLOQ:
        return TILEP_MONS_GLOORX_VLOQ;
    case MONS_GERYON:
        return TILEP_MONS_GERYON;
    case MONS_DISPATER:
        return TILEP_MONS_DISPATER;
    case MONS_ASMODEUS:
        return TILEP_MONS_ASMODEUS;
    case MONS_ERESHKIGAL:
        return TILEP_MONS_ERESHKIGAL;
    }

    return TILEP_MONS_PROGRAM_BUG;
}

int tileidx_monster(const monsters *mons, bool detected)
{
    int ch = tileidx_monster_base(mons, detected);
    // XXX: Treat this tile specially, as monsters normally
    //      don't use tile sets.
    if (ch == TILEP_MONS_STATUE_GUARDIAN)
        ch += + random2(tile_player_count(ch));

    if (mons_flies(mons))
        ch |= TILE_FLAG_FLYING;
    if (mons->has_ench(ENCH_HELD))
        ch |= TILE_FLAG_NET;
    if (mons->has_ench(ENCH_POISON))
        ch |= TILE_FLAG_POISON;
    if (mons->has_ench(ENCH_STICKY_FLAME))
        ch |= TILE_FLAG_FLAME;
    if (mons->berserk())
        ch |= TILE_FLAG_BERSERK;

    if (mons->friendly())
        ch |= TILE_FLAG_PET;
    else if (mons->good_neutral())
        ch |= TILE_FLAG_GD_NEUTRAL;
    else if (mons->neutral())
        ch |= TILE_FLAG_NEUTRAL;
    else if (mons_looks_stabbable(mons))
        ch |= TILE_FLAG_STAB;
    else if (mons_looks_distracted(mons))
        ch |= TILE_FLAG_MAY_STAB;

    std::string damage_desc;
    mon_dam_level_type damage_level;
    mons_get_damage_level(mons, damage_desc, damage_level);

    // If no messages about wounds, don't display an icon either.
    if (monster_descriptor(mons->type, MDSC_NOMSG_WOUNDS)
        || mons_is_unknown_mimic(mons))
    {
        damage_level = MDAM_OKAY;
    }

    switch (damage_level)
    {
    case MDAM_DEAD:
    case MDAM_ALMOST_DEAD:
        ch |= TILE_FLAG_MDAM_ADEAD;
        break;
    case MDAM_SEVERELY_DAMAGED:
        ch |= TILE_FLAG_MDAM_SEV;
        break;
    case MDAM_HEAVILY_DAMAGED:
        ch |= TILE_FLAG_MDAM_HEAVY;
        break;
    case MDAM_MODERATELY_DAMAGED:
        ch |= TILE_FLAG_MDAM_MOD;
        break;
    case MDAM_LIGHTLY_DAMAGED:
        ch |= TILE_FLAG_MDAM_LIGHT;
        break;
    case MDAM_OKAY:
    default:
        // no flag for okay.
        break;
    }

    if (Options.tile_show_demon_tier)
    {
        switch (mons_base_char(mons->type))
        {
        case '1':
            ch |= TILE_FLAG_DEMON_1;
            break;
        case '2':
            ch |= TILE_FLAG_DEMON_2;
            break;
        case '3':
            ch |= TILE_FLAG_DEMON_3;
            break;
        case '4':
            ch |= TILE_FLAG_DEMON_4;
            break;
        case '5':
            ch |= TILE_FLAG_DEMON_5;
            break;
        }
    }

    return ch;
}

static int _tileidx_monster(int mon_idx, bool detected)
{
    ASSERT(mon_idx != -1);
    const monsters* mons = &menv[mon_idx];
    return tileidx_monster(mons, detected);
}

static int _tileidx_unrand_artefact(int idx)
{
    const int tile = unrandart_to_tile(idx);
    if (tile != -1)
        return tile;

    return TILE_TODO;
}

static int _get_etype(const item_def &item)
{
    switch (item.flags & ISFLAG_COSMETIC_MASK)
    {
        case ISFLAG_EMBROIDERED_SHINY:
            return 1;
        case ISFLAG_RUNED:
            return 2;
        case ISFLAG_GLOWING:
            return 3;
        default:
            return (is_random_artefact(item) ? 4 : 0);
    }
}

static int _apply_variations(const item_def &item, int tile)
{
    static const int etable[5][5] =
    {
      {0, 0, 0, 0, 0},  // all variants look the same
      {0, 1, 1, 1, 1},  // normal, ego/randart
      {0, 1, 1, 1, 2},  // normal, ego, randart
      {0, 1, 1, 2, 3},  // normal, ego (shiny/runed), ego (glowing), randart
      {0, 1, 2, 3, 4}   // normal, shiny, runed, glowing, randart
    };

    const int etype = _get_etype(item);
    const int idx   = tile_main_count(tile) - 1;
    ASSERT(idx < 5);

    tile += etable[idx][etype];

    return (tile);
}

static int _tileidx_weapon_base(const item_def &item)
{
    int race  = item.flags & ISFLAG_RACIAL_MASK;

    switch (item.sub_type)
    {
    case WPN_KNIFE: return TILE_WPN_KNIFE;

    case WPN_DAGGER:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_DAGGER_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_DAGGER_ELF;
        return TILE_WPN_DAGGER;

    case WPN_SHORT_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_SHORT_SWORD_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_SHORT_SWORD_ELF;
        return TILE_WPN_SHORT_SWORD;

    case WPN_QUICK_BLADE: return TILE_WPN_QUICK_BLADE;
    case WPN_SABRE: return TILE_WPN_SABRE;
    case WPN_FALCHION: return TILE_WPN_FALCHION;
    case WPN_KATANA: return TILE_WPN_KATANA;

    case WPN_LONG_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_LONG_SWORD_ORC;
        return TILE_WPN_LONG_SWORD;

    case WPN_GREAT_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_GREAT_SWORD_ORC;
        return TILE_WPN_GREAT_SWORD;

    case WPN_SCIMITAR:
        return TILE_WPN_SCIMITAR;

    case WPN_DOUBLE_SWORD:
        return TILE_WPN_DOUBLE_SWORD;

    case WPN_TRIPLE_SWORD:
        return TILE_WPN_TRIPLE_SWORD;

    case WPN_HAND_AXE:
        return TILE_WPN_HAND_AXE;

    case WPN_WAR_AXE:
        return TILE_WPN_WAR_AXE;

    case WPN_BROAD_AXE:
        return TILE_WPN_BROAD_AXE;

    case WPN_BATTLEAXE:
        return TILE_WPN_BATTLEAXE;

    case WPN_EXECUTIONERS_AXE:
        return TILE_WPN_EXECUTIONERS_AXE;

    case WPN_BLOWGUN:
        return TILE_WPN_BLOWGUN;

    case WPN_SLING:
        return TILE_WPN_SLING;

    case WPN_BOW:
        return TILE_WPN_BOW;

    case WPN_CROSSBOW:
        return TILE_WPN_CROSSBOW;

    case WPN_SPEAR:
        return TILE_WPN_SPEAR;

    case WPN_TRIDENT:
        return TILE_WPN_TRIDENT;

    case WPN_HALBERD:
        return TILE_WPN_HALBERD;

    case WPN_SCYTHE:
        return TILE_WPN_SCYTHE;

    case WPN_GLAIVE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_GLAIVE_ORC;
        return TILE_WPN_GLAIVE;

    case WPN_QUARTERSTAFF:
        return TILE_WPN_QUARTERSTAFF;

    case WPN_CLUB:
        return TILE_WPN_CLUB;

    case WPN_HAMMER:
        return TILE_WPN_HAMMER;

    case WPN_MACE:
        return TILE_WPN_MACE;

    case WPN_FLAIL:
        return TILE_WPN_FLAIL;

    case WPN_SPIKED_FLAIL:
        return TILE_WPN_SPIKED_FLAIL;

    case WPN_GREAT_MACE:
        return TILE_WPN_GREAT_MACE;

    case WPN_DIRE_FLAIL:
        return TILE_WPN_GREAT_FLAIL;

    case WPN_MORNINGSTAR:
        return TILE_WPN_MORNINGSTAR;

    case WPN_EVENINGSTAR:
        return TILE_WPN_EVENINGSTAR;

    case WPN_GIANT_CLUB:
        return TILE_WPN_GIANT_CLUB;

    case WPN_GIANT_SPIKED_CLUB:
        return TILE_WPN_GIANT_SPIKED_CLUB;

    case WPN_ANKUS:
        return TILE_WPN_ANKUS;

    case WPN_WHIP:
        return TILE_WPN_WHIP;

    case WPN_HOLY_SCOURGE:
        return TILE_WPN_HOLY_SCOURGE;

    case WPN_DEMON_BLADE:
        return TILE_WPN_DEMON_BLADE;

    case WPN_DEMON_WHIP:
        return TILE_WPN_DEMON_WHIP;

    case WPN_DEMON_TRIDENT:
        return TILE_WPN_DEMON_TRIDENT;

    case WPN_HOLY_BLADE:
        return TILE_WPN_BLESSED_BLADE;

    case WPN_LONGBOW:
        return TILE_WPN_LONGBOW;

    case WPN_LAJATANG:
        return TILE_WPN_LAJATANG;

    case WPN_BARDICHE:
        return TILE_WPN_BARDICHE;

    case WPN_BLESSED_FALCHION:
        return TILE_WPN_FALCHION;

    case WPN_BLESSED_LONG_SWORD:
        return TILE_WPN_LONG_SWORD;

    case WPN_BLESSED_SCIMITAR:
        return TILE_WPN_SCIMITAR;

    case WPN_BLESSED_KATANA:
        return TILE_WPN_KATANA;

    case WPN_BLESSED_GREAT_SWORD:
        return TILE_WPN_GREAT_SWORD;

    case WPN_BLESSED_DOUBLE_SWORD:
        return TILE_WPN_DOUBLE_SWORD;

    case WPN_BLESSED_TRIPLE_SWORD:
        return TILE_WPN_TRIPLE_SWORD;
    }

    return TILE_ERROR;
}

static int _tileidx_weapon(const item_def &item)
{
    int tile = _tileidx_weapon_base(item);
    return _apply_variations(item, tile);
}

static int _tileidx_missile_base(const item_def &item)
{
    int brand = item.special;
    switch (item.sub_type)
    {
    case MI_STONE:        return TILE_MI_STONE;
    case MI_LARGE_ROCK:   return TILE_MI_LARGE_ROCK;
    case MI_THROWING_NET: return TILE_MI_THROWING_NET;


    case MI_DART:
        switch (brand)
        {
        default:             return TILE_MI_DART;
        case SPMSL_POISONED: return TILE_MI_DART_POISONED;
        case SPMSL_STEEL:    return TILE_MI_DART_STEEL;
        case SPMSL_SILVER:   return TILE_MI_DART_SILVER;
        }

    case MI_NEEDLE:
        if (brand == SPMSL_POISONED)
            return TILE_MI_NEEDLE_P;
        return TILE_MI_NEEDLE;

    case MI_ARROW:
        switch (brand)
        {
        default:             return TILE_MI_ARROW;
        case SPMSL_STEEL:    return TILE_MI_ARROW_STEEL;
        case SPMSL_SILVER:   return TILE_MI_ARROW_SILVER;
        }

    case MI_BOLT:
        switch (brand)
        {
        default:             return TILE_MI_BOLT;
        case SPMSL_STEEL:    return TILE_MI_BOLT_STEEL;
        case SPMSL_SILVER:   return TILE_MI_BOLT_SILVER;
        }

    case MI_SLING_BULLET:
        switch (brand)
        {
        default:             return TILE_MI_SLING_BULLET;
        case SPMSL_STEEL:    return TILE_MI_SLING_BULLET_STEEL;
        case SPMSL_SILVER:   return TILE_MI_SLING_BULLET_SILVER;
        }

    case MI_JAVELIN:
        switch (brand)
        {
        default:             return TILE_MI_JAVELIN;
        case SPMSL_STEEL:    return TILE_MI_JAVELIN_STEEL;
        case SPMSL_SILVER:   return TILE_MI_JAVELIN_SILVER;
        }
  }

  return TILE_ERROR;
}

static int _tileidx_missile(const item_def &item)
{
    int tile = _tileidx_missile_base(item);
    return (_apply_variations(item, tile));
}

static int _tileidx_armour_base(const item_def &item)
{
    int race  = item.flags & ISFLAG_RACIAL_MASK;
    int type  = item.sub_type;
    switch (type)
    {
    case ARM_ROBE:
        return TILE_ARM_ROBE;

    case ARM_LEATHER_ARMOUR:
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_LEATHER_ARMOUR_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_LEATHER_ARMOUR_ELF;
        return TILE_ARM_LEATHER_ARMOUR;

    case ARM_RING_MAIL:
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_RING_MAIL_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_RING_MAIL_ELF;
        if (race == ISFLAG_DWARVEN)
            return TILE_ARM_RING_MAIL_DWA;
        return TILE_ARM_RING_MAIL;

    case ARM_SCALE_MAIL:
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_SCALE_MAIL_ELF;
        return TILE_ARM_SCALE_MAIL;

    case ARM_CHAIN_MAIL:
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_CHAIN_MAIL_ELF;
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_CHAIN_MAIL_ORC;
        return TILE_ARM_CHAIN_MAIL;

    case ARM_SPLINT_MAIL:
        return TILE_ARM_SPLINT_MAIL;

    case ARM_BANDED_MAIL:
        return TILE_ARM_BANDED_MAIL;

    case ARM_PLATE_MAIL:
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_PLATE_MAIL_ORC;
        return TILE_ARM_PLATE_MAIL;

    case ARM_CRYSTAL_PLATE_MAIL:
        return TILE_ARM_CRYSTAL_PLATE_MAIL;

    case ARM_SHIELD:
        return TILE_ARM_SHIELD;

    case ARM_CLOAK:
        return TILE_ARM_CLOAK;

    case ARM_WIZARD_HAT:
        return TILE_THELM_WIZARD_HAT;

    case ARM_CAP:
        return TILE_THELM_CAP;

    case ARM_HELMET:
        return TILE_THELM_HELM;

    case ARM_GLOVES:
        return TILE_ARM_GLOVES;

    case ARM_BOOTS:
        return TILE_ARM_BOOTS;

    case ARM_BUCKLER:
        return TILE_ARM_BUCKLER;

    case ARM_LARGE_SHIELD:
        return TILE_ARM_LARGE_SHIELD;

    case ARM_CENTAUR_BARDING:
        return TILE_ARM_CENTAUR_BARDING;

    case ARM_NAGA_BARDING:
        return TILE_ARM_NAGA_BARDING;

    case ARM_ANIMAL_SKIN:
        return TILE_ARM_ANIMAL_SKIN;

    case ARM_TROLL_HIDE:
        return TILE_ARM_TROLL_HIDE;

    case ARM_TROLL_LEATHER_ARMOUR:
        return TILE_ARM_TROLL_LEATHER_ARMOUR;

    case ARM_DRAGON_HIDE:
        return TILE_ARM_DRAGON_HIDE;

    case ARM_DRAGON_ARMOUR:
        return TILE_ARM_DRAGON_ARMOUR;

    case ARM_ICE_DRAGON_HIDE:
        return TILE_ARM_ICE_DRAGON_HIDE;

    case ARM_ICE_DRAGON_ARMOUR:
        return TILE_ARM_ICE_DRAGON_ARMOUR;

    case ARM_STEAM_DRAGON_HIDE:
        return TILE_ARM_STEAM_DRAGON_HIDE;

    case ARM_STEAM_DRAGON_ARMOUR:
        return TILE_ARM_STEAM_DRAGON_ARMOUR;

    case ARM_MOTTLED_DRAGON_HIDE:
        return TILE_ARM_MOTTLED_DRAGON_HIDE;

    case ARM_MOTTLED_DRAGON_ARMOUR:
        return TILE_ARM_MOTTLED_DRAGON_ARMOUR;

    case ARM_STORM_DRAGON_HIDE:
        return TILE_ARM_STORM_DRAGON_HIDE;

    case ARM_STORM_DRAGON_ARMOUR:
        return TILE_ARM_STORM_DRAGON_ARMOUR;

    case ARM_GOLD_DRAGON_HIDE:
        return TILE_ARM_GOLD_DRAGON_HIDE;

    case ARM_GOLD_DRAGON_ARMOUR:
        return TILE_ARM_GOLD_DRAGON_ARMOUR;

    case ARM_SWAMP_DRAGON_HIDE:
        return TILE_ARM_SWAMP_DRAGON_HIDE;

    case ARM_SWAMP_DRAGON_ARMOUR:
        return TILE_ARM_SWAMP_DRAGON_ARMOUR;
    }

    return TILE_ERROR;
}

static int _tileidx_armour(const item_def &item)
{
    int tile = _tileidx_armour_base(item);
    return _apply_variations(item, tile);
}

static int _tileidx_chunk(const item_def &item)
{
    if (food_is_rotten(item))
    {
        if (!is_inedible(item))
        {
            if (is_poisonous(item))
                return TILE_FOOD_CHUNK_ROTTEN_POISONED;

            if (is_mutagenic(item))
                return TILE_FOOD_CHUNK_ROTTEN_MUTAGENIC;

            if (causes_rot(item))
                return TILE_FOOD_CHUNK_ROTTEN_ROTTING;

            if (is_forbidden_food(item))
                return TILE_FOOD_CHUNK_ROTTEN_FORBIDDEN;

            if (is_contaminated(item))
                return TILE_FOOD_CHUNK_ROTTEN_CONTAMINATED;
        }
        return TILE_FOOD_CHUNK_ROTTEN;
    }

    if (is_inedible(item))
        return TILE_FOOD_CHUNK;

    if (is_poisonous(item))
        return TILE_FOOD_CHUNK_POISONED;

    if (is_mutagenic(item))
        return TILE_FOOD_CHUNK_MUTAGENIC;

    if (causes_rot(item))
        return TILE_FOOD_CHUNK_ROTTING;

    if (is_forbidden_food(item))
        return TILE_FOOD_CHUNK_FORBIDDEN;

    if (is_contaminated(item))
        return TILE_FOOD_CHUNK_CONTAMINATED;

    return TILE_FOOD_CHUNK;
}

static int _tileidx_food(const item_def &item)
{
    switch (item.sub_type)
    {
    case FOOD_MEAT_RATION:  return TILE_FOOD_MEAT_RATION;
    case FOOD_BREAD_RATION: return TILE_FOOD_BREAD_RATION;
    case FOOD_PEAR:         return TILE_FOOD_PEAR;
    case FOOD_APPLE:        return TILE_FOOD_APPLE;
    case FOOD_CHOKO:        return TILE_FOOD_CHOKO;
    case FOOD_HONEYCOMB:    return TILE_FOOD_HONEYCOMB;
    case FOOD_ROYAL_JELLY:  return TILE_FOOD_ROYAL_JELLY;
    case FOOD_SNOZZCUMBER:  return TILE_FOOD_SNOZZCUMBER;
    case FOOD_PIZZA:        return TILE_FOOD_PIZZA;
    case FOOD_APRICOT:      return TILE_FOOD_APRICOT;
    case FOOD_ORANGE:       return TILE_FOOD_ORANGE;
    case FOOD_BANANA:       return TILE_FOOD_BANANA;
    case FOOD_STRAWBERRY:   return TILE_FOOD_STRAWBERRY;
    case FOOD_RAMBUTAN:     return TILE_FOOD_RAMBUTAN;
    case FOOD_LEMON:        return TILE_FOOD_LEMON;
    case FOOD_GRAPE:        return TILE_FOOD_GRAPE;
    case FOOD_SULTANA:      return TILE_FOOD_SULTANA;
    case FOOD_LYCHEE:       return TILE_FOOD_LYCHEE;
    case FOOD_BEEF_JERKY:   return TILE_FOOD_BEEF_JERKY;
    case FOOD_CHEESE:       return TILE_FOOD_CHEESE;
    case FOOD_SAUSAGE:      return TILE_FOOD_SAUSAGE;
    case FOOD_CHUNK:        return _tileidx_chunk(item);
    }

    return TILE_ERROR;
}

// Returns index of corpse tiles.
// Parameter item holds the corpse.
static int _tileidx_corpse(const item_def &item)
{
    const int type = item.plus;

    switch (type)
    {
    // insects ('a')
    case MONS_GIANT_COCKROACH:
        return TILE_CORPSE_GIANT_COCKROACH;
    case MONS_GIANT_ANT:
        return TILE_CORPSE_GIANT_ANT;
    case MONS_SOLDIER_ANT:
        return TILE_CORPSE_SOLDIER_ANT;
    case MONS_QUEEN_ANT:
        return TILE_CORPSE_QUEEN_ANT;

    // batty monsters ('b')
    case MONS_GIANT_BAT:
        return TILE_CORPSE_GIANT_BAT;
    case MONS_BUTTERFLY:
        return TILE_CORPSE_BUTTERFLY;

    // centaurs ('c')
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        return TILE_CORPSE_CENTAUR;
    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        return TILE_CORPSE_YAKTAUR;

    // draconians ('d')
    case MONS_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_BROWN;
    case MONS_BLACK_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_BLACK;
    case MONS_YELLOW_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_YELLOW;
    case MONS_PALE_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_PALE;
    case MONS_GREEN_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_GREEN;
    case MONS_PURPLE_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_PURPLE;
    case MONS_RED_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_RED;
    case MONS_WHITE_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_WHITE;
    case MONS_MOTTLED_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_MOTTLED;
    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_DRACONIAN_SCORCHER:
        return TILE_CORPSE_DRACONIAN_BROWN;

    // elves ('e')
    case MONS_ELF:
    case MONS_DEEP_ELF_SOLDIER:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_BLADEMASTER:
    case MONS_DEEP_ELF_MASTER_ARCHER:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_SUMMONER:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_DEATH_MAGE:
        return TILE_CORPSE_ELF;

    // goblins ('g')
    case MONS_GOBLIN:
        return TILE_CORPSE_GOBLIN;
    case MONS_HOBGOBLIN:
        return TILE_CORPSE_HOBGOBLIN;
    case MONS_GNOLL:
        return TILE_CORPSE_GNOLL;

    // hounds and hogs ('h')
    case MONS_JACKAL:
        return TILE_CORPSE_JACKAL;
    case MONS_HOUND:
        return TILE_CORPSE_HOUND;
    case MONS_WARG:
        return TILE_CORPSE_WARG;
    case MONS_WOLF:
        return TILE_CORPSE_WOLF;
    case MONS_WAR_DOG:
        return TILE_CORPSE_WAR_DOG;
    case MONS_HOG:
        return TILE_CORPSE_HOG;
    case MONS_HELL_HOUND:
        return TILE_CORPSE_HELL_HOUND;
    case MONS_HELL_HOG:
        return TILE_CORPSE_HELL_HOG;

    // slugs ('j')
    case MONS_ELEPHANT_SLUG:
        return TILE_CORPSE_ELEPHANT_SLUG;
    case MONS_GIANT_SLUG:
        return TILE_CORPSE_GIANT_SLUG;
    case MONS_GIANT_SNAIL:
        return TILE_CORPSE_GIANT_SNAIL;

    // bees ('k')
    case MONS_KILLER_BEE:
        return TILE_CORPSE_KILLER_BEE;
    case MONS_BUMBLEBEE:
        return TILE_CORPSE_BUMBLEBEE;
    case MONS_QUEEN_BEE:
        return TILE_CORPSE_QUEEN_BEE;

    // lizards ('l')
    case MONS_GIANT_NEWT:
        return TILE_CORPSE_GIANT_NEWT;
    case MONS_GIANT_GECKO:
        return TILE_CORPSE_GIANT_GECKO;
    case MONS_IGUANA:
        return TILE_CORPSE_IGUANA;
    case MONS_GILA_MONSTER:
        return TILE_CORPSE_GILA_MONSTER;
    case MONS_KOMODO_DRAGON:
        return TILE_CORPSE_KOMODO_DRAGON;

    // drakes (also 'l')
    case MONS_SWAMP_DRAKE:
        return TILE_CORPSE_SWAMP_DRAKE;
    case MONS_FIRE_DRAKE:
        return TILE_CORPSE_FIRE_DRAKE;
    case MONS_LINDWURM:
        return TILE_CORPSE_LINDWURM;
    case MONS_DEATH_DRAKE:
        return TILE_CORPSE_DEATH_DRAKE;

    // merfolk ('m')
    case MONS_MERFOLK:
        return TILE_CORPSE_MERFOLK;
    case MONS_MERMAID:
        return TILE_CORPSE_MERMAID;
    case MONS_SIREN:
        return TILE_CORPSE_SIREN;

    // rotting monsters ('n')
    case MONS_NECROPHAGE:
        return TILE_CORPSE_NECROPHAGE;
    case MONS_GHOUL:
        return TILE_CORPSE_GHOUL;
    case MONS_ROTTING_HULK:
        return TILE_CORPSE_ROTTING_HULK;

    // orcs ('o')
    case MONS_ORC:
    case MONS_ORC_WIZARD:
    case MONS_ORC_PRIEST:
    case MONS_ORC_WARRIOR:
    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARLORD:
    case MONS_ORC_SORCERER:
    case MONS_ORC_HIGH_PRIEST:
        return TILE_CORPSE_ORC;

    // rodents ('r')
    case MONS_RAT:
        return TILE_CORPSE_RAT;
    case MONS_QUOKKA:
        return TILE_CORPSE_QUOKKA;
    case MONS_GREY_RAT:
        return TILE_CORPSE_GREY_RAT;
    case MONS_GREEN_RAT:
        return TILE_CORPSE_GREEN_RAT;
    case MONS_ORANGE_RAT:
        return TILE_CORPSE_ORANGE_RAT;

    // spiders and insects ('s')
    case MONS_GIANT_MITE:
        return TILE_CORPSE_GIANT_MITE;
    case MONS_GIANT_CENTIPEDE:
        return TILE_CORPSE_GIANT_CENTIPEDE;
    case MONS_SCORPION:
        return TILE_CORPSE_SCORPION;
    case MONS_WOLF_SPIDER:
        return TILE_CORPSE_WOLF_SPIDER;
    case MONS_TRAPDOOR_SPIDER:
        return TILE_CORPSE_TRAPDOOR_SPIDER;
    case MONS_REDBACK:
        return TILE_CORPSE_REDBACK;
    case MONS_DEMONIC_CRAWLER:
        return TILE_CORPSE_DEMONIC_CRAWLER;

    // turtles and crocodiles ('t')
    case MONS_CROCODILE:
        return TILE_CORPSE_CROCODILE;
    case MONS_BABY_ALLIGATOR:
        return TILE_CORPSE_BABY_ALLIGATOR;
    case MONS_ALLIGATOR:
        return TILE_CORPSE_ALLIGATOR;
    case MONS_SNAPPING_TURTLE:
        return TILE_CORPSE_SNAPPING_TURTLE;
    case MONS_ALLIGATOR_SNAPPING_TURTLE:
        return TILE_CORPSE_ALLIGATOR_SNAPPING_TURTLE;

    // ugly things ('u')
    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        const int ugly_corpse_tile = (type == MONS_VERY_UGLY_THING) ?
            TILE_CORPSE_VERY_UGLY_THING : TILE_CORPSE_UGLY_THING;
        int colour_offset = ugly_thing_colour_offset(item.colour);

        if (colour_offset == -1)
            colour_offset = 0;

        return (ugly_corpse_tile + colour_offset);
    }

    // worms ('w')
    case MONS_KILLER_BEE_LARVA:
        return TILE_CORPSE_KILLER_BEE_LARVA;
    case MONS_ANT_LARVA:
        return TILE_CORPSE_ANT_LARVA;
    case MONS_WORM:
        return TILE_CORPSE_WORM;
    case MONS_BRAIN_WORM:
        return TILE_CORPSE_BRAIN_WORM;
    case MONS_SWAMP_WORM:
        return TILE_CORPSE_SWAMP_WORM;
    case MONS_GIANT_LEECH:
        return TILE_CORPSE_GIANT_LEECH;
    case MONS_ROCK_WORM:
        return TILE_CORPSE_ROCK_WORM;
    case MONS_SPINY_WORM:
        return TILE_CORPSE_SPINY_WORM;

    // flying insects ('y')
    case MONS_GIANT_MOSQUITO:
        return TILE_CORPSE_GIANT_MOSQUITO;
    case MONS_GIANT_BLOWFLY:
        return TILE_CORPSE_GIANT_BLOWFLY;
    case MONS_YELLOW_WASP:
        return TILE_CORPSE_YELLOW_WASP;
    case MONS_RED_WASP:
        return TILE_CORPSE_RED_WASP;
    case MONS_MOTH_OF_WRATH:
        return TILE_CORPSE_MOTH_OF_WRATH;

    // beetles ('B')
    case MONS_GIANT_BEETLE:
        return TILE_CORPSE_GIANT_BEETLE;
    case MONS_BOULDER_BEETLE:
        return TILE_CORPSE_BOULDER_BEETLE;
    case MONS_BORING_BEETLE:
        return TILE_CORPSE_BORING_BEETLE;

    // giants ('C')
    case MONS_HILL_GIANT:
        return TILE_CORPSE_HILL_GIANT;
    case MONS_ETTIN:
        return TILE_CORPSE_ETTIN;
    case MONS_CYCLOPS:
        return TILE_CORPSE_CYCLOPS;
    case MONS_FIRE_GIANT:
        return TILE_CORPSE_FIRE_GIANT;
    case MONS_FROST_GIANT:
        return TILE_CORPSE_FROST_GIANT;
    case MONS_STONE_GIANT:
        return TILE_CORPSE_STONE_GIANT;
    case MONS_TITAN:
        return TILE_CORPSE_TITAN;

    // dragons ('D')
    case MONS_WYVERN:
        return TILE_CORPSE_WYVERN;
    case MONS_DRAGON:
        return TILE_CORPSE_DRAGON;
    case MONS_HYDRA:
        return TILE_CORPSE_HYDRA;
    case MONS_ICE_DRAGON:
        return TILE_CORPSE_ICE_DRAGON;
    case MONS_IRON_DRAGON:
        return TILE_CORPSE_IRON_DRAGON;
    case MONS_QUICKSILVER_DRAGON:
        return TILE_CORPSE_QUICKSILVER_DRAGON;
    case MONS_STEAM_DRAGON:
        return TILE_CORPSE_STEAM_DRAGON;
    case MONS_SWAMP_DRAGON:
        return TILE_CORPSE_SWAMP_DRAGON;
    case MONS_MOTTLED_DRAGON:
        return TILE_CORPSE_MOTTLED_DRAGON;
    case MONS_STORM_DRAGON:
        return TILE_CORPSE_STORM_DRAGON;
    case MONS_GOLDEN_DRAGON:
        return TILE_CORPSE_GOLDEN_DRAGON;
    case MONS_SHADOW_DRAGON:
        return TILE_CORPSE_SHADOW_DRAGON;

    // frogs ('F')
    case MONS_GIANT_FROG:
        return TILE_CORPSE_GIANT_FROG;
    case MONS_GIANT_TOAD:
        return TILE_CORPSE_GIANT_TOAD;
    case MONS_SPINY_FROG:
        return TILE_CORPSE_SPINY_FROG;
    case MONS_BLINK_FROG:
        return TILE_CORPSE_BLINK_FROG;

    // eyes ('G')
    case MONS_GIANT_EYEBALL:
        return TILE_CORPSE_GIANT_EYEBALL;
    case MONS_EYE_OF_DEVASTATION:
        return TILE_CORPSE_EYE_OF_DEVASTATION;
    case MONS_EYE_OF_DRAINING:
        return TILE_CORPSE_EYE_OF_DRAINING;
    case MONS_GIANT_ORANGE_BRAIN:
        return TILE_CORPSE_GIANT_ORANGE_BRAIN;
    case MONS_GREAT_ORB_OF_EYES:
        return TILE_CORPSE_GREAT_ORB_OF_EYES;
    case MONS_SHINING_EYE:
        return TILE_CORPSE_SHINING_EYE;

    // hybrids ('H')
    case MONS_HIPPOGRIFF:
        return TILE_CORPSE_HIPPOGRIFF;
    case MONS_MANTICORE:
        return TILE_CORPSE_MANTICORE;
    case MONS_GRIFFON:
        return TILE_CORPSE_GRIFFON;
    case MONS_HARPY:
        return TILE_CORPSE_HARPY;
    case MONS_MINOTAUR:
        return TILE_CORPSE_MINOTAUR;
    case MONS_SPHINX:
        return TILE_CORPSE_SPHINX;

    // jellies ('J')
    case MONS_GIANT_AMOEBA:
        return TILE_CORPSE_GIANT_AMOEBA;

    // kobolds ('K')
    case MONS_KOBOLD:
        return TILE_CORPSE_KOBOLD;
    case MONS_BIG_KOBOLD:
        return TILE_CORPSE_BIG_KOBOLD;

    // nagas ('N')
    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GREATER_NAGA:
        return TILE_CORPSE_NAGA;
    case MONS_GUARDIAN_SERPENT:
        return TILE_CORPSE_GUARDIAN_SERPENT;

    // ogres ('O')
    case MONS_OGRE:
    case MONS_OGRE_MAGE:
        return TILE_CORPSE_OGRE;
    case MONS_TWO_HEADED_OGRE:
        return TILE_CORPSE_TWO_HEADED_OGRE;

    // snakes ('S')
    case MONS_SMALL_SNAKE:
        return TILE_CORPSE_SMALL_SNAKE;
    case MONS_SNAKE:
        return TILE_CORPSE_SNAKE;
    case MONS_ANACONDA:
        return TILE_CORPSE_ANACONDA;
    case MONS_WATER_MOCCASIN:
        return TILE_CORPSE_WATER_MOCCASIN;
    case MONS_BLACK_MAMBA:
        return TILE_CORPSE_BLACK_MAMBA;
    case MONS_VIPER:
        return TILE_CORPSE_VIPER;
    case MONS_SEA_SNAKE:
        return TILE_CORPSE_SEA_SNAKE;

    // trolls ('T')
    case MONS_TROLL:
        return TILE_CORPSE_TROLL;
    case MONS_ROCK_TROLL:
        return TILE_CORPSE_ROCK_TROLL;
    case MONS_IRON_TROLL:
        return TILE_CORPSE_IRON_TROLL;
    case MONS_DEEP_TROLL:
        return TILE_CORPSE_DEEP_TROLL;

    // bears ('U')
    case MONS_BEAR:
        return TILE_CORPSE_BEAR;
    case MONS_GRIZZLY_BEAR:
        return TILE_CORPSE_GRIZZLY_BEAR;
    case MONS_POLAR_BEAR:
        return TILE_CORPSE_POLAR_BEAR;
    case MONS_BLACK_BEAR:
        return TILE_CORPSE_BLACK_BEAR;

    // cattle ('Y')
    case MONS_SHEEP:
        return TILE_CORPSE_SHEEP;
    case MONS_YAK:
        return TILE_CORPSE_YAK;
    case MONS_DEATH_YAK:
        return TILE_CORPSE_DEATH_YAK;

    // water monsters
    case MONS_BIG_FISH:
        return TILE_CORPSE_BIG_FISH;
    case MONS_GIANT_GOLDFISH:
        return TILE_CORPSE_GIANT_GOLDFISH;
    case MONS_ELECTRIC_EEL:
        return TILE_CORPSE_ELECTRIC_EEL;
    case MONS_SHARK:
        return TILE_CORPSE_SHARK;
    case MONS_KRAKEN:
        return TILE_CORPSE_KRAKEN;
    case MONS_JELLYFISH:
        return TILE_CORPSE_JELLYFISH;

    // humans ('@')
    case MONS_HUMAN:
    case MONS_HELL_KNIGHT:
    case MONS_NECROMANCER:
    case MONS_WIZARD:
        return TILE_CORPSE_HUMAN;
    case MONS_SHAPESHIFTER:
        return TILE_CORPSE_SHAPESHIFTER;
    case MONS_GLOWING_SHAPESHIFTER:
        return TILE_CORPSE_GLOWING_SHAPESHIFTER;

    case MONS_DWARF:
        return TILE_CORPSE_DWARF;

    default:
        return TILE_ERROR;
    }
}

static int _tileidx_rune(const item_def &item)
{
    switch (item.plus)
    {
    // the hell runes:
    case RUNE_DIS:         return TILE_MISC_RUNE_DIS;
    case RUNE_GEHENNA:     return TILE_MISC_RUNE_GEHENNA;
    case RUNE_COCYTUS:     return TILE_MISC_RUNE_COCYTUS;
    case RUNE_TARTARUS:    return TILE_MISC_RUNE_TARTARUS;

    // special pandemonium runes:
    case RUNE_MNOLEG:      return TILE_MISC_RUNE_MNOLEG;
    case RUNE_LOM_LOBON:   return TILE_MISC_RUNE_LOM_LOBON;
    case RUNE_CEREBOV:     return TILE_MISC_RUNE_CEREBOV;
    case RUNE_GLOORX_VLOQ: return TILE_MISC_RUNE_GLOORX_VLOQ;

    // All others use the default rune for now.
    case RUNE_SLIME_PITS:
    case RUNE_VAULTS:
    case RUNE_SNAKE_PIT:
    case RUNE_ELVEN_HALLS:
    case RUNE_TOMB:
    case RUNE_SWAMP:
    case RUNE_SHOALS:

    // pandemonium and abyss runes:
    case RUNE_DEMONIC:
    case RUNE_ABYSSAL:
    default:               return TILE_MISC_RUNE_OF_ZOT;
    }
}

static int _tileidx_misc(const item_def &item)
{
    switch (item.sub_type)
    {
    case MISC_BOTTLED_EFREET:
        return TILE_MISC_BOTTLED_EFREET;

    case MISC_CRYSTAL_BALL_OF_SEEING:
        return TILE_MISC_CRYSTAL_BALL_OF_SEEING;

    case MISC_AIR_ELEMENTAL_FAN:
        return TILE_MISC_AIR_ELEMENTAL_FAN;

    case MISC_LAMP_OF_FIRE:
        return TILE_MISC_LAMP_OF_FIRE;

    case MISC_STONE_OF_EARTH_ELEMENTALS:
        return TILE_MISC_STONE_OF_EARTH_ELEMENTALS;

    case MISC_LANTERN_OF_SHADOWS:
        return TILE_MISC_LANTERN_OF_SHADOWS;

    case MISC_HORN_OF_GERYON:
        return TILE_MISC_HORN_OF_GERYON;

    case MISC_BOX_OF_BEASTS:
        return TILE_MISC_BOX_OF_BEASTS;

    case MISC_CRYSTAL_BALL_OF_ENERGY:
        return TILE_MISC_CRYSTAL_BALL_OF_ENERGY;

    case MISC_EMPTY_EBONY_CASKET:
        return TILE_MISC_EMPTY_EBONY_CASKET;

    case MISC_CRYSTAL_BALL_OF_FIXATION:
        return TILE_MISC_CRYSTAL_BALL_OF_FIXATION;

    case MISC_DISC_OF_STORMS:
        return TILE_MISC_DISC_OF_STORMS;

    case MISC_DECK_OF_ESCAPE:
    case MISC_DECK_OF_DESTRUCTION:
    case MISC_DECK_OF_DUNGEONS:
    case MISC_DECK_OF_SUMMONING:
    case MISC_DECK_OF_WONDERS:
    case MISC_DECK_OF_PUNISHMENT:
    case MISC_DECK_OF_WAR:
    case MISC_DECK_OF_CHANGES:
    case MISC_DECK_OF_DEFENCE:
    {
        int ch = TILE_ERROR;
        switch (item.special)
        {
        case DECK_RARITY_LEGENDARY:
            ch = TILE_MISC_DECK_LEGENDARY;
            break;
        case DECK_RARITY_RARE:
            ch = TILE_MISC_DECK_RARE;
            break;
        case DECK_RARITY_COMMON:
        default:
            ch = TILE_MISC_DECK;
            break;
        }
        if (item.flags & ISFLAG_KNOW_TYPE)
        {
            // NOTE: order of tiles must be identical to order of decks.
            int offset = item.sub_type - MISC_DECK_OF_ESCAPE + 1;
            ch += offset;
        }
        return ch;
    }
    case MISC_RUNE_OF_ZOT:
        return _tileidx_rune(item);
    }

    return TILE_ERROR;
}

/*****************************************************/
int tileidx_item(const item_def &item)
{
    int clas    = item.base_type;
    int type    = item.sub_type;
    int special = item.special;
    int colour  = item.colour;

    id_arr& id = get_typeid_array();

    switch (clas)
    {
    case OBJ_WEAPONS:
        if (is_unrandom_artefact( item ))
            return _tileidx_unrand_artefact(find_unrandart_index(item));
        else
            return _tileidx_weapon(item);

    case OBJ_MISSILES:
        return _tileidx_missile(item);

    case OBJ_ARMOUR:
        if (is_unrandom_artefact( item ))
            return _tileidx_unrand_artefact(find_unrandart_index(item));
        else
            return _tileidx_armour(item);

    case OBJ_WANDS:
        if (id[ IDTYPE_WANDS ][type] == ID_KNOWN_TYPE
            ||  (item.flags &ISFLAG_KNOW_TYPE ))
        {
            return TILE_WAND_FLAME + type;
        }
        else
            return TILE_WAND_OFFSET + special % 12;

    case OBJ_FOOD:
        return _tileidx_food(item);

    case OBJ_SCROLLS:
        if (id[ IDTYPE_SCROLLS ][type] == ID_KNOWN_TYPE
            ||  (item.flags &ISFLAG_KNOW_TYPE ))
        {
            return TILE_SCR_IDENTIFY + type;
        }
        return TILE_SCROLL;

    case OBJ_GOLD:
        return TILE_GOLD;

    case OBJ_JEWELLERY:
        if (type < NUM_RINGS)
        {
            if (is_random_artefact( item ))
                return TILE_RING_RANDOM_OFFSET + colour - 1;
            else if (id[ IDTYPE_JEWELLERY][type] == ID_KNOWN_TYPE
                     || (item.flags & ISFLAG_KNOW_TYPE))
            {
                return TILE_RING_REGENERATION + type - RING_FIRST_RING;
            }
            else
            {
                return TILE_RING_NORMAL_OFFSET + special % 13;
            }
        }
        else
        {
            if (is_unrandom_artefact( item ))
                return _tileidx_unrand_artefact(find_unrandart_index(item));
            else if (is_random_artefact( item ))
                return TILE_AMU_RANDOM_OFFSET + colour - 1;
            else if (id[ IDTYPE_JEWELLERY][type] == ID_KNOWN_TYPE
                     || (item.flags & ISFLAG_KNOW_TYPE))
            {
                return TILE_AMU_RAGE + type - AMU_FIRST_AMULET;
            }
            else
            {
                return TILE_AMU_NORMAL_OFFSET + special % 13;
            }
        }

    case OBJ_POTIONS:
        if (id[ IDTYPE_POTIONS ][type] == ID_KNOWN_TYPE
            ||  (item.flags &ISFLAG_KNOW_TYPE ))
        {
            return TILE_POT_HEALING + type;
        }
        else
        {
            return TILE_POTION_OFFSET + item.plus % 14;
        }

    case OBJ_BOOKS:
        if (is_random_artefact(item))
        {
            int offset = special % tile_main_count(TILE_BOOK_RANDART_OFFSET);
            return TILE_BOOK_RANDART_OFFSET + offset;
        }

        switch (special % 10)
        {
        default:
        case 0:
        case 1:
            return TILE_BOOK_PAPER_OFFSET + colour;
        case 2:
            return TILE_BOOK_LEATHER_OFFSET + special/10;
        case 3:
            return TILE_BOOK_METAL_OFFSET + special/10;
        case 4:
            return TILE_BOOK_PAPYRUS;
        }

    case OBJ_STAVES:
        if (item_is_rod(item))
        {
            if (id[IDTYPE_STAVES][type] == ID_KNOWN_TYPE
                ||  (item.flags & ISFLAG_KNOW_TYPE ))
            {
                return TILE_ROD_SMITING + type - STAFF_SMITING;
            }
            return TILE_ROD_OFFSET + (special / 4) % 10;
        }
        else
        {
            if (id[IDTYPE_STAVES][type] == ID_KNOWN_TYPE
                ||  (item.flags & ISFLAG_KNOW_TYPE ))
            {
                return TILE_STAFF_WIZARDRY + type;
            }
            return TILE_STAFF_OFFSET + (special / 4) % 10;
        }

    case OBJ_CORPSES:
        if (item.sub_type == CORPSE_SKELETON)
            return TILE_FOOD_BONE;
        else
            return _tileidx_corpse(item);

    case OBJ_ORBS:
        return TILE_ORB + random2(tile_main_count(TILE_ORB));

    case OBJ_MISCELLANY:
        return _tileidx_misc(item);

    default:
        return TILE_ERROR;
    }
}

//  Determine Octant of missile direction
//   .---> X+
//   |
//   |  701
//   Y  6O2
//   +  543
//
// The octant boundary slope tan(pi/8)=sqrt(2)-1 = 0.414 is approximated by 2/5.
static int _tile_bolt_dir(int dx, int dy)
{
    int ax = abs(dx);
    int ay = abs(dy);

    if (5*ay < 2*ax)
        return (dx > 0) ? 2 : 6;
    else if (5*ax < 2*ay)
        return (dy > 0) ? 4 : 0;
    else if (dx > 0)
        return (dy > 0) ? 3 : 1;
    else
        return (dy > 0) ? 5: 7;
}

int tileidx_item_throw(const item_def &item, int dx, int dy)
{
    if (item.base_type == OBJ_MISSILES)
    {
        int ch = -1;
        int dir = _tile_bolt_dir(dx, dy);

        // Thrown items with multiple directions
        switch (item.sub_type)
        {
            case MI_ARROW:
                ch = TILE_MI_ARROW0;
                break;
            case MI_BOLT:
                ch = TILE_MI_BOLT0;
                break;
            case MI_DART:
                ch = TILE_MI_DART0;
                break;
            case MI_NEEDLE:
                ch = TILE_MI_NEEDLE0;
                break;
            case MI_JAVELIN:
                ch = TILE_MI_JAVELIN0;
                break;
            case MI_THROWING_NET:
                ch = TILE_MI_THROWING_NET0;
            default:
                break;
        }
        if (ch != -1)
            return ch + dir;

        // Thrown items with a single direction
        switch (item.sub_type)
        {
            case MI_STONE:
                ch = TILE_MI_STONE0;
                break;
            case MI_SLING_BULLET:
                switch (item.special)
                {
                default:
                    ch = TILE_MI_SLING_BULLET0;
                    break;
                case SPMSL_STEEL:
                    ch = TILE_MI_SLING_BULLET_STEEL0;
                    break;
                case SPMSL_SILVER:
                    ch = TILE_MI_SLING_BULLET_SILVER0;
                    break;
                }
                break;
            case MI_LARGE_ROCK:
                ch = TILE_MI_LARGE_ROCK0;
                break;
            case MI_THROWING_NET:
                ch = TILE_MI_THROWING_NET0;
                break;
            default:
                break;
        }
        if (ch != -1)
            return (_apply_variations(item, ch));
    }

    // If not a special case, just return the default tile.
    return tileidx_item(item);
}

static int _tileidx_trap(trap_type type)
{
    switch (type)
    {
    case TRAP_DART:
        return TILE_DNGN_TRAP_DART;
    case TRAP_ARROW:
        return TILE_DNGN_TRAP_ARROW;
    case TRAP_SPEAR:
        return TILE_DNGN_TRAP_SPEAR;
    case TRAP_AXE:
        return TILE_DNGN_TRAP_AXE;
    case TRAP_TELEPORT:
        return TILE_DNGN_TRAP_TELEPORT;
    case TRAP_ALARM:
        return TILE_DNGN_TRAP_ALARM;
    case TRAP_BLADE:
        return TILE_DNGN_TRAP_BLADE;
    case TRAP_BOLT:
        return TILE_DNGN_TRAP_BOLT;
    case TRAP_NET:
        return TILE_DNGN_TRAP_NET;
    case TRAP_ZOT:
        return TILE_DNGN_TRAP_ZOT;
    case TRAP_NEEDLE:
        return TILE_DNGN_TRAP_NEEDLE;
    case TRAP_SHAFT:
        return TILE_DNGN_TRAP_SHAFT;
    default:
        return TILE_DNGN_ERROR;
    }
}

static int _tileidx_shop(coord_def where)
{
    const shop_struct *shop = get_shop(where);
    if (!shop)
        return TILE_DNGN_ERROR;

    switch (shop->type)
    {
        case SHOP_WEAPON:
        case SHOP_WEAPON_ANTIQUE:
            return TILE_SHOP_WEAPONS;
        case SHOP_ARMOUR:
        case SHOP_ARMOUR_ANTIQUE:
            return TILE_SHOP_ARMOUR;
        case SHOP_JEWELLERY:
            return TILE_SHOP_JEWELLERY;
        case SHOP_WAND:
            return TILE_SHOP_WANDS;
        case SHOP_FOOD:
            return TILE_SHOP_FOOD;
        case SHOP_BOOK:
            return TILE_SHOP_BOOKS;
        case SHOP_SCROLL:
            return TILE_SHOP_SCROLLS;
        case SHOP_DISTILLERY:
            return TILE_SHOP_POTIONS;
        case SHOP_GENERAL:
        case SHOP_GENERAL_ANTIQUE:
            return TILE_SHOP_GENERAL;
        default:
            return TILE_DNGN_ERROR;
    }
}

int tileidx_feature(dungeon_feature_type feat, int gx, int gy)
{
    int override = env.tile_flv[gx][gy].feat;
    bool can_override = !feat_is_door(grd[gx][gy])
                        && feat != DNGN_FLOOR
                        && feat != DNGN_UNSEEN;
    if (override && can_override)
        return (override);

    switch (feat)
    {
    case DNGN_UNSEEN:
        return TILE_DNGN_UNSEEN;
    case DNGN_FLOOR_SPECIAL:
    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
        return TILE_WALL_NORMAL;
    case DNGN_SLIMY_WALL:
        return TILE_WALL_SLIME;
    case DNGN_OPEN_SEA:
        return TILE_DNGN_OPEN_SEA;
    case DNGN_SECRET_DOOR:
    case DNGN_DETECTED_SECRET_DOOR:
    {
        dungeon_feature_type dfeat;
        coord_def door;
        if (find_secret_door_info(coord_def(gx, gy), &dfeat, &door))
            return tileidx_feature(grid_appearance(door), door.x, door.y);
        else
            return dfeat;
    }
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_CLEAR_STONE_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
        return TILE_DNGN_TRANSPARENT_WALL;
    case DNGN_STONE_WALL:
        return TILE_DNGN_STONE_WALL;
    case DNGN_CLOSED_DOOR:
        return TILE_DNGN_CLOSED_DOOR;
    case DNGN_METAL_WALL:
        return TILE_DNGN_METAL_WALL;
    case DNGN_GREEN_CRYSTAL_WALL:
        return TILE_DNGN_GREEN_CRYSTAL_WALL;
    case DNGN_ORCISH_IDOL:
        return TILE_DNGN_ORCISH_IDOL;
    case DNGN_WAX_WALL:
        return TILE_DNGN_WAX_WALL;
    case DNGN_TREE:
        return TILE_DNGN_TREE;
    case DNGN_GRANITE_STATUE:
        return TILE_DNGN_GRANITE_STATUE;
    case DNGN_LAVA:
        return TILE_DNGN_LAVA;
    case DNGN_DEEP_WATER:
        if (env.grid_colours[gx][gy] == GREEN
            || env.grid_colours[gx][gy] == LIGHTGREEN)
        {
            return TILE_DNGN_DEEP_WATER_MURKY;
        }
        else if (player_in_branch(BRANCH_SHOALS))
            return TILE_SHOALS_DEEP_WATER;

        return TILE_DNGN_DEEP_WATER;
    case DNGN_SHALLOW_WATER:
    {
        int t = TILE_DNGN_SHALLOW_WATER;
        if (env.grid_colours[gx][gy] == GREEN
            || env.grid_colours[gx][gy] == LIGHTGREEN)
        {
            t = TILE_DNGN_SHALLOW_WATER_MURKY;
        }
        else if (player_in_branch(BRANCH_SHOALS))
            t = TILE_SHOALS_SHALLOW_WATER;

        monsters *mon = monster_at(coord_def(gx, gy));
        if (mon)
        {
            // Add disturbance to tile.
            if (mon->submerged())
                t += tile_dngn_count(t);
        }

        return (t);
    }
    case DNGN_FLOOR:
    case DNGN_UNDISCOVERED_TRAP:
        return TILE_FLOOR_NORMAL;
    case DNGN_ENTER_HELL:
        return TILE_DNGN_ENTER_HELL;
    case DNGN_OPEN_DOOR:
        return TILE_DNGN_OPEN_DOOR;
    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_NATURAL:
        return _tileidx_trap(get_trap_type(coord_def(gx, gy)));
    case DNGN_ENTER_SHOP:
        return _tileidx_shop(coord_def(gx,gy));
    case DNGN_ABANDONED_SHOP:
        return TILE_DNGN_ABANDONED_SHOP;
    case DNGN_ENTER_LABYRINTH:
        return TILE_DNGN_ENTER_LABYRINTH;
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        return TILE_DNGN_STONE_STAIRS_DOWN;
    case DNGN_ESCAPE_HATCH_DOWN:
        return TILE_DNGN_ESCAPE_HATCH_DOWN;
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        if (player_in_hell())
            return TILE_DNGN_RETURN_HELL;
        return TILE_DNGN_STONE_STAIRS_UP;
    case DNGN_ESCAPE_HATCH_UP:
        return TILE_DNGN_ESCAPE_HATCH_UP;
    case DNGN_ENTER_DIS:
        return TILE_DNGN_ENTER_DIS;
    case DNGN_ENTER_GEHENNA:
        return TILE_DNGN_ENTER_GEHENNA;
    case DNGN_ENTER_COCYTUS:
        return TILE_DNGN_ENTER_COCYTUS;
    case DNGN_ENTER_TARTARUS:
        return TILE_DNGN_ENTER_TARTARUS;
    case DNGN_ENTER_ABYSS:
        return TILE_DNGN_ENTER_ABYSS;
    case DNGN_EXIT_ABYSS:
    case DNGN_EXIT_HELL:
    case DNGN_EXIT_PANDEMONIUM:
        return TILE_DNGN_EXIT_ABYSS;
    case DNGN_STONE_ARCH:
        return TILE_DNGN_STONE_ARCH;
    case DNGN_ENTER_PANDEMONIUM:
        return TILE_DNGN_ENTER_PANDEMONIUM;
    case DNGN_TRANSIT_PANDEMONIUM:
        return TILE_DNGN_TRANSIT_PANDEMONIUM;
    case DNGN_ENTER_ORCISH_MINES:
    case DNGN_ENTER_HIVE:
    case DNGN_ENTER_LAIR:
    case DNGN_ENTER_SLIME_PITS:
    case DNGN_ENTER_VAULTS:
    case DNGN_ENTER_CRYPT:
    case DNGN_ENTER_HALL_OF_BLADES:
    case DNGN_ENTER_TEMPLE:
    case DNGN_ENTER_SNAKE_PIT:
    case DNGN_ENTER_ELVEN_HALLS:
    case DNGN_ENTER_TOMB:
    case DNGN_ENTER_SWAMP:
    case DNGN_ENTER_SHOALS:
        return TILE_DNGN_ENTER;

    case DNGN_ENTER_ZOT:
        if (you.opened_zot)
            return TILE_DNGN_ENTER_ZOT_OPEN;
        return TILE_DNGN_ENTER_ZOT_CLOSED;

    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_ELVEN_HALLS:
    case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_SHOALS:
        return TILE_DNGN_RETURN;
    case DNGN_RETURN_FROM_ZOT:
        return TILE_DNGN_RETURN_ZOT;
    case DNGN_ENTER_PORTAL_VAULT:
    case DNGN_EXIT_PORTAL_VAULT:
        return TILE_DNGN_PORTAL;

    // altars
    case DNGN_ALTAR_ZIN:
        return TILE_DNGN_ALTAR_ZIN;
    case DNGN_ALTAR_SHINING_ONE:
        return TILE_DNGN_ALTAR_SHINING_ONE;
    case DNGN_ALTAR_KIKUBAAQUDGHA:
        return TILE_DNGN_ALTAR_KIKUBAAQUDGHA;
    case DNGN_ALTAR_YREDELEMNUL:
        return TILE_DNGN_ALTAR_YREDELEMNUL;
    case DNGN_ALTAR_XOM:
        return TILE_DNGN_ALTAR_XOM;
    case DNGN_ALTAR_VEHUMET:
        return TILE_DNGN_ALTAR_VEHUMET;
    case DNGN_ALTAR_OKAWARU:
        return TILE_DNGN_ALTAR_OKAWARU;
    case DNGN_ALTAR_MAKHLEB:
        return TILE_DNGN_ALTAR_MAKHLEB;
    case DNGN_ALTAR_SIF_MUNA:
        return TILE_DNGN_ALTAR_SIF_MUNA;
    case DNGN_ALTAR_TROG:
        return TILE_DNGN_ALTAR_TROG;
    case DNGN_ALTAR_NEMELEX_XOBEH:
        return TILE_DNGN_ALTAR_NEMELEX_XOBEH;
    case DNGN_ALTAR_ELYVILON:
        return TILE_DNGN_ALTAR_ELYVILON;
    case DNGN_ALTAR_LUGONU:
        return TILE_DNGN_ALTAR_LUGONU;
    case DNGN_ALTAR_BEOGH:
        return TILE_DNGN_ALTAR_BEOGH;
    case DNGN_ALTAR_JIYVA:
        return TILE_DNGN_ALTAR_JIYVA;
    case DNGN_ALTAR_FEDHAS:
        return TILE_DNGN_ALTAR_FEDHAS;
    case DNGN_ALTAR_CHEIBRIADOS:
        return TILE_DNGN_ALTAR_CHEIBRIADOS;
    case DNGN_FOUNTAIN_BLUE:
        return TILE_DNGN_BLUE_FOUNTAIN;
    case DNGN_FOUNTAIN_SPARKLING:
        return TILE_DNGN_SPARKLING_FOUNTAIN;
    case DNGN_FOUNTAIN_BLOOD:
        return TILE_DNGN_BLOOD_FOUNTAIN;
    case DNGN_DRY_FOUNTAIN_BLUE:
    case DNGN_DRY_FOUNTAIN_SPARKLING:
    case DNGN_DRY_FOUNTAIN_BLOOD:
    case DNGN_PERMADRY_FOUNTAIN:
        return TILE_DNGN_DRY_FOUNTAIN;
    default:
        return TILE_DNGN_ERROR;
    }
}

static int _tileidx_cloud(cloud_struct cl)
{
    int type  = cl.type;
    int decay = cl.decay;
    std::string override = cl.tile;
    int colour = cl.colour;

    int ch  = TILE_ERROR;
    int dur = decay/20;
    if (dur < 0)
        dur = 0;
    else if (dur > 2)
        dur = 2;

    if (!override.empty())
    {
        unsigned int index;
        if (!tile_main_index(override.c_str(), index))
        {
            mprf(MSGCH_ERROR, "Invalid tile requested for cloud: '%s'.", override.c_str());
        }
        else
        {
            int offset = tile_main_count(index);
            ch = index + offset;
        }
    }
    else
    {
        switch (type)
        {
            case CLOUD_FIRE:
                ch = TILE_CLOUD_FIRE_0 + dur;
                break;

            case CLOUD_COLD:
                ch = TILE_CLOUD_COLD_0 + dur;
                break;

            case CLOUD_STINK:
            case CLOUD_POISON:
                ch = TILE_CLOUD_POISON_0 + dur;
                break;

            case CLOUD_BLUE_SMOKE:
                ch = TILE_CLOUD_BLUE_SMOKE;
                break;

            case CLOUD_PURPLE_SMOKE:
            case CLOUD_TLOC_ENERGY:
                ch = TILE_CLOUD_TLOC_ENERGY;
                break;

            case CLOUD_MIASMA:
                ch = TILE_CLOUD_MIASMA;
                break;

            case CLOUD_BLACK_SMOKE:
                ch = TILE_CLOUD_BLACK_SMOKE;
                break;

            case CLOUD_MUTAGENIC:
                ch = (dur == 0 ? TILE_CLOUD_MUTAGENIC_0 :
                      dur == 1 ? TILE_CLOUD_MUTAGENIC_1
                               : TILE_CLOUD_MUTAGENIC_2);
                ch += random2(tile_main_count(ch));
                break;

            case CLOUD_MIST:
                ch = TILE_CLOUD_MIST;
                break;

            case CLOUD_RAIN:
                ch = TILE_CLOUD_RAIN + random2(tile_main_count(TILE_CLOUD_RAIN));
                break;

            case CLOUD_MAGIC_TRAIL:
                if (decay/20 > 2)
                    dur = 3;
                ch = TILE_CLOUD_MAGIC_TRAIL_0 + dur;
                break;

            case CLOUD_INK:
                ch = TILE_CLOUD_INK;
                break;

            case CLOUD_GLOOM:
                ch = TILE_CLOUD_GLOOM;
                break;

            default:
                ch = TILE_CLOUD_GREY_SMOKE;
                break;
        }
    }

    if (colour != -1)
        ch = tile_main_coloured(ch, colour);

    return (ch | TILE_FLAG_FLYING);
}

/**********************************************************/

int tileidx_player(int job)
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


void tileidx_unseen(unsigned int &fg, unsigned int &bg, int ch,
                    const coord_def& gc)
{
    ch &= 0xff;
    if (ch < 32)
        ch = 32;

    unsigned int flag = tile_unseen_flag(gc);
    fg = 0;
    bg = TILE_FLOOR_NORMAL | flag;

    if (ch >= '@' && ch <= 'Z' || ch >= 'a' && ch <= 'z'
        || ch == '&' || ch >= '1' && ch <= '5' || ch == ';')
    {
        fg = TILE_UNSEEN_MONSTER;
        return;
    }

    switch (ch)
    {
        //blank, walls, and floors first, since they are frequent
        case ' ': bg = TILE_DNGN_UNSEEN; break;
        case 127: //old
        case 176:
        case 177: bg = TILE_WALL_NORMAL; break;

        case 130:
        case ',':
        case '.':
        case 249:
        case 250: bg = TILE_FLOOR_NORMAL; break;

        case 137: bg = TILE_DNGN_WAX_WALL; break;
        case 138: bg = TILE_DNGN_STONE_WALL; break;
        case 139: bg = TILE_DNGN_METAL_WALL; break;
        case 140: bg = TILE_DNGN_GREEN_CRYSTAL_WALL; break;

        // others
        case '!': fg = TILE_POTION_OFFSET + 13; break;
        case '"': fg = TILE_AMU_NORMAL_OFFSET + 2; break;
        case '#': fg = TILE_CLOUD_GREY_SMOKE; break;
        case '$': fg = TILE_GOLD; break;
        case '%': fg = TILE_FOOD_MEAT_RATION; break;
        case 142: fg = TILE_UNSEEN_CORPSE; break;

        case '\'':
        case 134: bg = TILE_DNGN_OPEN_DOOR; break;
        case '(':
        case ')': fg = TILE_UNSEEN_WEAPON; break;
        case '*': fg = TILE_WALL_NORMAL ; break;
        case '+': fg = TILE_BOOK_PAPER_OFFSET + 15; break;

        case '/': fg = TILE_WAND_OFFSET; break;
        case '8': fg = TILEP_MONS_SILVER_STATUE; break;
        case '<': bg = TILE_DNGN_STONE_STAIRS_UP; break;
        case '=': fg = TILE_RING_NORMAL_OFFSET + 1; break;
        case '>': bg = TILE_DNGN_STONE_STAIRS_DOWN; break;
        case '~':
        case '?': fg = TILE_UNSEEN_ITEM; break;
        case '[':
        case ']': fg = TILE_UNSEEN_ARMOUR; break;
        case '\\': fg = TILE_STAFF_OFFSET; break;
        case '^': bg = TILE_DNGN_TRAP_ARROW; break;
        case '_':
        case 220:
        case 131: bg = TILE_DNGN_UNSEEN_ALTAR; break;
        case '{':
        case 247:
        case 135: bg = TILE_DNGN_DEEP_WATER; break;
        case 244:
        case 133: bg = TILE_DNGN_BLUE_FOUNTAIN; break;
        case '}': fg = TILE_MISC_CRYSTAL_BALL_OF_SEEING; break;
        case 128: //old
        case 254: bg = TILE_DNGN_CLOSED_DOOR; break;
        case 129: bg = TILE_DNGN_RETURN; break;
        case 239:
        case 132: bg = TILE_DNGN_UNSEEN_ENTRANCE; break;
        case 136: bg = TILE_DNGN_ENTER; break;
        case 141: bg = TILE_DNGN_LAVA; break;
    }

    bg |= flag;
}

int tileidx_bolt(const bolt &bolt)
{
    const int col = bolt.colour;
    return tileidx_zap(col);
}

int tileidx_zap(int colour)
{
    int col = (colour == ETC_MAGIC ? element_colour(ETC_MAGIC) : colour);

    if (col > 8)
        col -= 8;
    if (col < 1)
        col = 7;
    return (TILE_SYM_BOLT_OFS - 1 + col);
}

int tileidx_spell(spell_type spell)
{
    switch (spell)
    {
    case SPELL_NO_SPELL:
    case SPELL_DEBUGGING_RAY:
        return TILEG_ERROR;

    case NUM_SPELLS: // XXX: Hack!
        return TILEG_MEMORISE;

    // Air
    case SPELL_SHOCK:                    return TILEG_SHOCK;
    case SPELL_SWIFTNESS:                return TILEG_SWIFTNESS;
    case SPELL_LEVITATION:               return TILEG_LEVITATION;
    case SPELL_REPEL_MISSILES:           return TILEG_REPEL_MISSILES;
    case SPELL_MEPHITIC_CLOUD:           return TILEG_MEPHITIC_CLOUD;
    case SPELL_DISCHARGE:                return TILEG_STATIC_DISCHARGE;
    case SPELL_FLY:                      return TILEG_FLIGHT;
    case SPELL_INSULATION:               return TILEG_INSULATION;
    case SPELL_LIGHTNING_BOLT:           return TILEG_LIGHTNING_BOLT;
    case SPELL_AIRSTRIKE:                return TILEG_AIRSTRIKE;
    case SPELL_SILENCE:                  return TILEG_SILENCE;
    case SPELL_DEFLECT_MISSILES:         return TILEG_DEFLECT_MISSILES;
    case SPELL_CONJURE_BALL_LIGHTNING:   return TILEG_CONJURE_BALL_LIGHTNING;
    case SPELL_CHAIN_LIGHTNING:          return TILEG_CHAIN_LIGHTNING;

    // Earth
    case SPELL_SANDBLAST:                return TILEG_SANDBLAST;
    case SPELL_MAXWELLS_SILVER_HAMMER:   return TILEG_MAXWELLS_SILVER_HAMMER;
    case SPELL_STONESKIN:                return TILEG_STONESKIN;
    case SPELL_PASSWALL:                 return TILEG_PASSWALL;
    case SPELL_STONE_ARROW:              return TILEG_STONE_ARROW;
    case SPELL_DIG:                      return TILEG_DIG;
    case SPELL_BOLT_OF_MAGMA:            return TILEG_BOLT_OF_MAGMA;
    case SPELL_FRAGMENTATION:            return TILEG_LEES_RAPID_DECONSTRUCTION;
    case SPELL_IRON_SHOT:                return TILEG_IRON_SHOT;
    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:   return TILEG_LEHUDIBS_CRYSTAL_SPEAR;
    case SPELL_SHATTER:                  return TILEG_SHATTER;

    // Fire
    case SPELL_FLAME_TONGUE:             return TILEG_FLAME_TONGUE;
    case SPELL_EVAPORATE:                return TILEG_EVAPORATE;
    case SPELL_FIRE_BRAND:               return TILEG_FIRE_BRAND;
    case SPELL_THROW_FLAME:              return TILEG_THROW_FLAME;
    case SPELL_CONJURE_FLAME:            return TILEG_CONJURE_FLAME;
    case SPELL_STICKY_FLAME:             return TILEG_STICKY_FLAME;
    case SPELL_BOLT_OF_FIRE:             return TILEG_BOLT_OF_FIRE;
    case SPELL_IGNITE_POISON:            return TILEG_IGNITE_POISON;
    case SPELL_FIREBALL:                 return TILEG_FIREBALL;
    case SPELL_DELAYED_FIREBALL:         return TILEG_DELAYED_FIREBALL;
    case SPELL_RING_OF_FLAMES:           return TILEG_RING_OF_FLAMES;
    case SPELL_FIRE_STORM:               return TILEG_FIRE_STORM;

    // Ice
    case SPELL_FREEZE:                   return TILEG_FREEZE;
    case SPELL_THROW_FROST:              return TILEG_THROW_FROST;
    case SPELL_FREEZING_AURA:            return TILEG_FREEZING_AURA;
    case SPELL_HIBERNATION:              return TILEG_ENSORCELLED_HIBERNATION;
    case SPELL_OZOCUBUS_ARMOUR:          return TILEG_OZOCUBUS_ARMOUR;
    case SPELL_THROW_ICICLE:             return TILEG_THROW_ICICLE;
    case SPELL_CONDENSATION_SHIELD:      return TILEG_CONDENSATION_SHIELD;
    case SPELL_OZOCUBUS_REFRIGERATION:   return TILEG_OZOCUBUS_REFRIGERATION;
    case SPELL_BOLT_OF_COLD:             return TILEG_BOLT_OF_COLD;
    case SPELL_FREEZING_CLOUD:           return TILEG_FREEZING_CLOUD;
    case SPELL_ENGLACIATION:             return TILEG_METABOLIC_ENGLACIATION;
    case SPELL_SIMULACRUM:               return TILEG_SIMULACRUM;
    case SPELL_ICE_STORM:                return TILEG_ICE_STORM;

    // Poison
    case SPELL_STING:                    return TILEG_STING;
    case SPELL_CURE_POISON:              return TILEG_CURE_POISON;
    case SPELL_POISON_WEAPON:            return TILEG_POISON_BRAND;
    case SPELL_INTOXICATE:               return TILEG_ALISTAIRS_INTOXICATION;
    case SPELL_OLGREBS_TOXIC_RADIANCE:   return TILEG_OLGREBS_TOXIC_RADIANCE;
    case SPELL_RESIST_POISON:            return TILEG_RESIST_POISON;
    case SPELL_VENOM_BOLT:               return TILEG_VENOM_BOLT;
    case SPELL_POISON_ARROW:             return TILEG_POISON_ARROW;
    case SPELL_POISONOUS_CLOUD:          return TILEG_POISONOUS_CLOUD;

    // Enchantment
    case SPELL_CONFUSING_TOUCH:          return TILEG_CONFUSING_TOUCH;
    case SPELL_CORONA:                   return TILEG_CORONA;
    case SPELL_PROJECTED_NOISE:          return TILEG_PROJECTED_NOISE;
    case SPELL_SURE_BLADE:               return TILEG_SURE_BLADE;
    case SPELL_TUKIMAS_VORPAL_BLADE:     return TILEG_TUKIMAS_VORPAL_BLADE;
    case SPELL_BERSERKER_RAGE:           return TILEG_BERSERKER_RAGE;
    case SPELL_CONFUSE:                  return TILEG_CONFUSE;
    case SPELL_SLOW:                     return TILEG_SLOW;
    case SPELL_TUKIMAS_DANCE:            return TILEG_TUKIMAS_DANCE;
    case SPELL_SELECTIVE_AMNESIA:        return TILEG_SELECTIVE_AMNESIA;
    case SPELL_ENSLAVEMENT:              return TILEG_ENSLAVEMENT;
    case SPELL_SEE_INVISIBLE:            return TILEG_SEE_INVISIBLE;
    case SPELL_PETRIFY:                  return TILEG_PETRIFY;
    case SPELL_CAUSE_FEAR:               return TILEG_CAUSE_FEAR;
    case SPELL_TAME_BEASTS:              return TILEG_TAME_BEASTS;
    case SPELL_EXTENSION:                return TILEG_EXTENSION;
    case SPELL_HASTE:                    return TILEG_HASTE;
    case SPELL_INVISIBILITY:             return TILEG_INVISIBILITY;
    case SPELL_MASS_CONFUSION:           return TILEG_MASS_CONFUSION;

    // Translocation
    case SPELL_APPORTATION:              return TILEG_APPORTATION;
    case SPELL_PORTAL_PROJECTILE:        return TILEG_PORTAL_PROJECTILE;
    case SPELL_BLINK:                    return TILEG_BLINK;
    case SPELL_BANISHMENT:               return TILEG_BANISHMENT;
    case SPELL_CONTROL_TELEPORT:         return TILEG_CONTROLLED_TELEPORT;
    case SPELL_TELEPORT_OTHER:           return TILEG_TELEPORT_OTHER;
    case SPELL_TELEPORT_SELF:            return TILEG_TELEPORT;
    case SPELL_PHASE_SHIFT:              return TILEG_PHASE_SHIFT;
    case SPELL_CONTROLLED_BLINK:         return TILEG_CONTROLLED_BLINK;
    case SPELL_WARP_BRAND:               return TILEG_WARP_WEAPON;
    case SPELL_DISPERSAL:                return TILEG_DISPERSAL;
    case SPELL_PORTAL:                   return TILEG_PORTAL;

    // Summoning
    case SPELL_SUMMON_BUTTERFLIES:       return TILEG_SUMMON_BUTTERFLIES;
    case SPELL_SUMMON_SMALL_MAMMALS:     return TILEG_SUMMON_SMALL_MAMMALS;
    case SPELL_RECALL:                   return TILEG_RECALL;
    case SPELL_CALL_CANINE_FAMILIAR:     return TILEG_CALL_CANINE_FAMILIAR;
    case SPELL_CALL_IMP:                 return TILEG_CALL_IMP;
    case SPELL_ABJURATION:               return TILEG_ABJURATION;
    case SPELL_SUMMON_SCORPIONS:         return TILEG_SUMMON_SCORPIONS;
    case SPELL_SUMMON_ELEMENTAL:         return TILEG_SUMMON_ELEMENTAL;
    case SPELL_SUMMON_DEMON:             return TILEG_SUMMON_DEMON;
    case SPELL_SUMMON_UGLY_THING:        return TILEG_SUMMON_UGLY_THING;
    case SPELL_SHADOW_CREATURES:         return TILEG_SUMMON_SHADOW_CREATURES;
    case SPELL_SUMMON_ICE_BEAST:         return TILEG_SUMMON_ICE_BEAST;
    case SPELL_DEMONIC_HORDE:            return TILEG_DEMONIC_HORDE;
    case SPELL_SUMMON_GREATER_DEMON:     return TILEG_SUMMON_GREATER_DEMON;
    case SPELL_SUMMON_HORRIBLE_THINGS:   return TILEG_SUMMON_HORRIBLE_THINGS;

    // Necromancy
    case SPELL_ANIMATE_SKELETON:         return TILEG_ANIMATE_SKELETON;
    case SPELL_PAIN:                     return TILEG_PAIN;
    case SPELL_FULSOME_DISTILLATION:     return TILEG_FULSOME_DISTILLATION;
    case SPELL_CORPSE_ROT:               return TILEG_CORPSE_ROT;
    case SPELL_LETHAL_INFUSION:          return TILEG_LETHAL_INFUSION;
    case SPELL_SUBLIMATION_OF_BLOOD:     return TILEG_SUBLIMATION_OF_BLOOD;
    case SPELL_BONE_SHARDS:              return TILEG_BONE_SHARDS;
    case SPELL_VAMPIRIC_DRAINING:        return TILEG_VAMPIRIC_DRAINING;
    case SPELL_REGENERATION:             return TILEG_REGENERATION;
    case SPELL_ANIMATE_DEAD:             return TILEG_ANIMATE_DEAD;
    case SPELL_DISPEL_UNDEAD:            return TILEG_DISPEL_UNDEAD;
    case SPELL_HAUNT:                    return TILEG_HAUNT;
    case SPELL_BORGNJORS_REVIVIFICATION: return TILEG_BORGNJORS_REVIVIFICATION;
    case SPELL_CIGOTUVIS_DEGENERATION:   return TILEG_CIGOTUVIS_DEGENERATION;
    case SPELL_AGONY:                    return TILEG_AGONY;
    case SPELL_TWISTED_RESURRECTION:     return TILEG_TWISTED_RESURRECTION;
    case SPELL_EXCRUCIATING_WOUNDS:      return TILEG_EXCRUCIATING_WOUNDS;
    case SPELL_CONTROL_UNDEAD:           return TILEG_CONTROL_UNDEAD;
    case SPELL_BOLT_OF_DRAINING:         return TILEG_BOLT_OF_DRAINING;
    case SPELL_SYMBOL_OF_TORMENT:        return TILEG_SYMBOL_OF_TORMENT;
    case SPELL_DEATHS_DOOR:              return TILEG_DEATHS_DOOR;
    case SPELL_DEATH_CHANNEL:            return TILEG_DEATH_CHANNEL;

    // Transmutation
    case SPELL_STICKS_TO_SNAKES:         return TILEG_STICKS_TO_SNAKES;
    case SPELL_SPIDER_FORM:              return TILEG_SPIDER_FORM;
    case SPELL_ICE_FORM:                 return TILEG_ICE_FORM;
    case SPELL_BLADE_HANDS:              return TILEG_BLADE_HANDS;
    case SPELL_POLYMORPH_OTHER:          return TILEG_POLYMORPH_OTHER;
    case SPELL_STATUE_FORM:              return TILEG_STATUE_FORM;
    case SPELL_ALTER_SELF:               return TILEG_ALTER_SELF;
    case SPELL_DRAGON_FORM:              return TILEG_DRAGON_FORM;
    case SPELL_NECROMUTATION:            return TILEG_NECROMUTATION;

    // pure Conjuration
    case SPELL_MAGIC_DART:               return TILEG_MAGIC_DART;
    case SPELL_ISKENDERUNS_MYSTIC_BLAST: return TILEG_ISKENDERUNS_MYSTIC_BLAST;
    case SPELL_IOOD:                     return TILEG_IOOD;

    // Divination (soon to be obsolete, or moved to abilities)
    case SPELL_DETECT_SECRET_DOORS:      return TILEG_DETECT_SECRET_DOORS;
    case SPELL_DETECT_TRAPS:             return TILEG_DETECT_TRAPS;
    case SPELL_DETECT_ITEMS:             return TILEG_DETECT_ITEMS;
    case SPELL_DETECT_CREATURES:         return TILEG_DETECT_CREATURES;

    // --------------------------------------------
    // Rods and abilities (tiles needed for later)
    // Abilities
    case SPELL_SMITING:       // Beogh power
    case SPELL_MINOR_HEALING: // Ely power
    case SPELL_MAJOR_HEALING: // Ely power
    case SPELL_HELLFIRE:      // Demonspawn ability

    // Rod-only spells
    case SPELL_PARALYSE:      // we could reuse Petrify for this
    case SPELL_STRIKING:
    case SPELL_BOLT_OF_INACCURACY:
    case SPELL_SUMMON_SWARM:
        return TILEG_TODO;

    // --------------------------------------------
    // Spells that don't need icons:
    case SPELL_STONEMAIL:     // Xom only?
    case SPELL_SUMMON_DRAGON: // Xom
    case SPELL_DISINTEGRATE:  // wand and card

    // Monster spells (mostly?)
    case SPELL_HELLFIRE_BURST:
    case SPELL_VAMPIRE_SUMMON:
    case SPELL_BRAIN_FEED:
    case SPELL_FAKE_RAKSHASA_SUMMON:
    case SPELL_STEAM_BALL:
    case SPELL_SUMMON_UFETUBUS:
    case SPELL_SUMMON_BEAST:
    case SPELL_ENERGY_BOLT:
    case SPELL_POISON_SPLASH:
    case SPELL_SUMMON_UNDEAD:
    case SPELL_CANTRIP:
    case SPELL_QUICKSILVER_BOLT:
    case SPELL_METAL_SPLINTERS:
    case SPELL_MIASMA:
    case SPELL_SUMMON_DRAKES:
    case SPELL_BLINK_OTHER:
    case SPELL_SUMMON_MUSHROOMS:
    case SPELL_ACID_SPLASH:
    case SPELL_STICKY_FLAME_SPLASH:
    case SPELL_FIRE_BREATH:
    case SPELL_COLD_BREATH:
    case SPELL_DRACONIAN_BREATH:
    case SPELL_WATER_ELEMENTALS:
    case SPELL_PORKALATOR:
    default:
        return TILEG_ERROR;
    }
}

// Specifically for vault-overwritten doors. We have three "sets" of tiles that
// can be dealt with. The tile sets should be 2, 3, 8 and 9 respectively. They
// are:
//  2. Closed, open.
//  3. Detected, closed, open.
//  8. Closed, open, gate left closed, gate middle closed, gate right closed,
//     gate left open, gate middle open, gate right open.
//  9. Detected, closed, open, gate left closed, gate middle closed, gate right
//     closed, gate left open, gate middle open, gate right open.
int _get_door_offset (int base_tile, bool opened = false,
                      bool detected = false, int gateway_type = 0)
{
    int count = tile_dngn_count(base_tile);
    if (count == 1)
        return 0;

    // The location of the default "closed" tile.
    int offset = 0;

    switch (count)
    {
    case 2:
        return ((opened) ? 1: 0);
    case 3:
        if (opened)
            return 2;
        else if (detected)
            return 0;
        else
            return 1;
    case 8:
        // But is BASE_TILE for others.
        offset = 0;
        break;
    case 9:
        // It's located at BASE_TILE+1 for tile sets with detected doors
        offset = 1;
        break;
    default:
        // Passed a non-door tile base, pig out now.
        ASSERT(false);
    }

    // If we've reached this point, we're dealing with a gate.
    // Don't believe gateways deal differently with detection.
    if (detected)
        return 0;

    if (!opened && !detected && gateway_type == 0)
        return 0;

    return offset + gateway_type;
}

static int _pick_random_dngn_tile(unsigned int idx, int value = -1)
{
    ASSERT(idx >= 0 && idx < TILE_DNGN_MAX);
    const int count = tile_dngn_count(idx);
    if (count == 1)
        return (idx);

    const int total = tile_dngn_probs(idx + count - 1);
    const int rand  = (value == -1 ? random2(total) : value % total);

    for (int i = 0; i < count; ++i)
    {
        int curr = idx + i;
        if (rand < tile_dngn_probs(curr))
            return (curr);
    }

    return (idx);
}

// Updates the "flavour" of tiles that are animated.
void tile_apply_animations(screen_buffer_t bg, tile_flavour *flv)
{
    int bg_idx = bg & TILE_FLAG_MASK;
    if (bg_idx >= TILE_DNGN_LAVA && bg_idx < TILE_BLOOD)
    {
        flv->special = random2(256);
    }
    else if (bg_idx == TILE_DNGN_PORTAL_WIZARD_LAB
             || bg_idx == TILE_DNGN_ALTAR_CHEIBRIADOS)
    {
        flv->special = (flv->special + 1) % tile_dngn_count(bg_idx);
    }
}

static inline void _apply_variations(const tile_flavour &flv,
                                     screen_buffer_t *bg)
{
    screen_buffer_t orig = (*bg) & TILE_FLAG_MASK;
    screen_buffer_t flag = (*bg) & (~TILE_FLAG_MASK);

    // TODO enne - expose this as an option, so ziggurat can use it too.
    // Alternatively, allow the stone type to be set.
    //
    // Hack: Swap rock/stone in crypt and tomb, because there are
    //       only stone walls.
    if ((you.where_are_you == BRANCH_CRYPT || you.where_are_you == BRANCH_TOMB)
        && orig == TILE_DNGN_STONE_WALL)
    {
        orig = TILE_WALL_NORMAL;
    }

    if (orig == TILE_FLOOR_NORMAL)
        *bg = flv.floor;
    else if (orig == TILE_WALL_NORMAL)
        *bg = flv.wall;
    else if (orig == TILE_DNGN_CLOSED_DOOR || orig == TILE_DNGN_OPEN_DOOR)
    {
        int override = flv.feat;
        // Setting an override on a door specifically for undetected secret
        // doors causes issues if there are a number of variants for that tile.
        // In these instances, append "last_tile" and have the tile specifier
        // for the door in question on its own line, and it should bypass any
        // asserts or weird visual issues. Somewhat hackish. The following code
        // assumes that if there is an override on a door location and that
        // has no variations, that the override is not actually a door tile but
        // the aforementioned secret door thing. {due}
        if (override && tile_dngn_count(override) > 1)
        {
            // XXX: This doesn't deal properly with detected doors.
            bool opened = (orig == TILE_DNGN_OPEN_DOOR);
            int offset = _get_door_offset(override, opened, false, flv.special);
            *bg = override + offset;
        }
        else
            *bg = orig + std::min((int)flv.special, 3);
    }
    else if (orig == TILE_DNGN_PORTAL_WIZARD_LAB
             || orig == TILE_DNGN_ALTAR_CHEIBRIADOS)
    {
        *bg = orig + flv.special;
    }
    else if (orig < TILE_DNGN_MAX)
    {
        *bg = _pick_random_dngn_tile(orig, flv.special);
    }

    *bg |= flag;
}

void tilep_calc_flags(const int parts[], int flag[])
{
    for (unsigned i = 0; i < TILEP_PART_MAX; i++)
        flag[i] = TILEP_FLAG_NORMAL;

    if (parts[TILEP_PART_HELM] - 1 >= TILEP_HELM_HELM_OFS)
        flag[TILEP_PART_HAIR] = TILEP_FLAG_HIDE;

    if (parts[TILEP_PART_HELM] - 1 >= TILEP_HELM_FHELM_OFS)
        flag[TILEP_PART_BEARD] = TILEP_FLAG_HIDE;

    if (is_player_tile(parts[TILEP_PART_BASE], TILEP_BASE_NAGA))
    {
        flag[TILEP_PART_BOOTS] = flag[TILEP_PART_LEG] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_CUT_NAGA;
    }
    else if (is_player_tile(parts[TILEP_PART_BASE], TILEP_BASE_CENTAUR))
    {
        flag[TILEP_PART_BOOTS] = flag[TILEP_PART_LEG] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_CUT_CENTAUR;
    }
    else if (is_player_tile(parts[TILEP_PART_BASE], TILEP_BASE_MERFOLK_WATER))
    {
        flag[TILEP_PART_BOOTS]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_LEG]    = TILEP_FLAG_HIDE;
        flag[TILEP_PART_SHADOW] = TILEP_FLAG_HIDE;
    }
    else if (parts[TILEP_PART_BASE] >= TILEP_BASE_DRACONIAN
             && parts[TILEP_PART_BASE] <= TILEP_BASE_DRACONIAN_WHITE + 1)
    {
        flag[TILEP_PART_HAIR] = flag[TILEP_PART_HELM] = TILEP_FLAG_HIDE;
    }
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
        case MONS_GREEN_DRACONIAN:  return (4);
        case MONS_MOTTLED_DRACONIAN:return (5);
        case MONS_PALE_DRACONIAN:   return (6);
        case MONS_PURPLE_DRACONIAN: return (7);
        case MONS_RED_DRACONIAN:    return (8);
        case MONS_WHITE_DRACONIAN:  return (9);
        }
    }
    if (level < 7)
        return (0);
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

int get_gender_from_tile(const int parts[])
{
    const int gender = (parts[TILEP_PART_BASE]
                        - tile_player_part_start[TILEP_PART_BASE]) % 2;

    if (gender == TILEP_GENDER_MALE || gender == TILEP_GENDER_FEMALE)
        return gender;

    return TILEP_GENDER_FEMALE;
}

bool is_player_tile(const int tile, const int base_tile)
{
    return (tile >= base_tile
            && tile < base_tile + tile_player_count(base_tile));
}

int tilep_species_to_base_tile(int sp, int level)
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
    default:
        return TILEP_BASE_HUMAN;
    }
}

void tilep_draconian_init(int sp, int level, int &base, int &head, int &wing)
{
    const int colour_offset = _draconian_colour(sp, level);
    base  = TILEP_BASE_DRACONIAN + colour_offset * 2;
    head  = tile_player_part_start[TILEP_PART_DRCHEAD] + colour_offset;

    if (player_mutation_level(MUT_BIG_WINGS))
        wing = tile_player_part_start[TILEP_PART_DRCWING] + colour_offset;
}

// Set default parts of each race: body + optional beard, hair, etc.
void tilep_race_default(int sp, int gender, int level, int *parts)
{
    if (gender == -1)
        gender = get_gender_from_tile(parts);

    int result = tilep_species_to_base_tile(sp, level);
    if (parts[TILEP_PART_BASE] != TILEP_SHOW_EQUIP)
        result = (parts[TILEP_PART_BASE] - gender);

    int hair   = 0;
    int beard  = 0;
    int wing   = 0;
    int head   = 0;

    if (gender == TILEP_GENDER_MALE)
        hair = TILEP_HAIR_SHORT_BLACK;
    else
        hair = TILEP_HAIR_LONG_BLACK;

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
            if (gender == TILEP_GENDER_MALE)
            {
                hair  = TILEP_HAIR_SHORT_RED;
                beard = TILEP_BEARD_LONG_RED;
            }
            else
            {
                hair  = TILEP_HAIR_LONG_RED;
                beard = TILEP_BEARD_SHORT_RED;
            }
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
            tilep_draconian_init(sp, level, result, head, wing);
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
            break;
        case SP_VAMPIRE:
            if (gender == TILEP_GENDER_MALE)
                hair = TILEP_HAIR_ARAGORN;
            else
                hair = TILEP_HAIR_ARWEN;
            break;
        case SP_DEEP_DWARF:
            if (gender == TILEP_GENDER_MALE)
            {
                hair  = TILEP_HAIR_SHORT_WHITE;
                beard = TILEP_BEARD_LONG_WHITE;
            }
            else
            {
                hair  = TILEP_HAIR_FEM_WHITE;
                beard = TILEP_BEARD_SHORT_WHITE;
            }
            break;
        default:
            // nothing to do
            break;
    }

    parts[TILEP_PART_BASE] = result + gender;

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

void tilep_job_default(int job, int gender, int *parts)
{
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
            parts[TILEP_PART_HELM]  = TILEP_HELM_HELM_OFS;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_GRAY;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_GRAY;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLUE;
            break;

        case JOB_PALADIN:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_WHITE;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_HELM_OFS;
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
            parts[TILEP_PART_HELM]  = TILEP_HELM_GANDALF;
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
            parts[TILEP_PART_HELM]  = TILEP_HELM_GANDALF;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_STAFF_MAGE2;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_RED_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_ENCHANTER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_YELLOW;
            parts[TILEP_PART_HELM]  = TILEP_HELM_GANDALF;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_STAFF_MAGE;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_BLUE_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_SUMMONER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_GANDALF;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_STAFF_RING_BLUE;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_YELLOW_DIM;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_WARPER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_GANDALF;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_SARUMAN;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_WHITE;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_RED;
            break;


        case JOB_ARCANE_MARKSMAN:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_GANDALF;
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

            if (gender == TILEP_GENDER_MALE)
            {
                parts[TILEP_PART_BODY]  = TILEP_BODY_BELT1;
                parts[TILEP_PART_LEG]   = TILEP_LEG_BELT_GRAY;
                parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_GRAY;
            }
            else
            {
                parts[TILEP_PART_BODY]  = TILEP_BODY_BIKINI_RED;
                parts[TILEP_PART_LEG]   = TILEP_LEG_BIKINI_RED;
                parts[TILEP_PART_BOOTS] = TILEP_BOOTS_LONG_RED;
            }
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

/*
 * Parts index to string
 */
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

/*
 * Parts string to index
 */
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

        const int idx = tilep_str_to_part(ibuf);
        if (idx == TILEP_SHOW_EQUIP)
            doll.parts[p] = TILEP_SHOW_EQUIP;
        else if (p == TILEP_PART_BASE)
        {
            const int base_tile = tilep_species_to_base_tile(species, level);
            if (idx >= tile_player_count(base_tile))
                doll.parts[p] = base_tile + (idx % 2);
            else
                doll.parts[p] = base_tile + idx;
        }
        else if (idx == 0)
            doll.parts[p] = 0;
        else if (idx > tile_player_part_count[p])
            doll.parts[p] = tile_player_part_start[p];
        else
        {
            const int idx2 = tile_player_part_start[p] + idx - 1;
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
        int idx = doll.parts[p];
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
    ptr--; // erase the last ':'
    *ptr = 0;
}

/*
 * return equip-dependent parts index
 */
int tilep_equ_weapon(const item_def &item)
{
    if (item.base_type == OBJ_STAVES)
    {
        if (item_is_rod(item))
            return TILEP_HAND1_ROD_BROWN + (item.special / 4) % 10;
        else
            return TILEP_HAND1_STAFF_LARGE + (item.special / 4) % 10;
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

    if (is_unrandom_artefact( item ))
    {
        const int tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile != -1)
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
    case WPN_HOLY_SCOURGE:      return TILEP_HAND1_HOLY_SCOURGE;

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
    case WPN_HOLY_BLADE:           return TILEP_HAND1_BLESSED_BLADE;
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
    case WPN_SCYTHE:        return TILEP_HAND1_SCYTHE;
    case WPN_HAMMER:        return TILEP_HAND1_HAMMER;
    case WPN_DEMON_TRIDENT: return TILEP_HAND1_DEMON_TRIDENT;
    case WPN_TRIDENT:       return TILEP_HAND1_TRIDENT2;
    case WPN_LAJATANG:      return TILEP_HAND1_DIRE_LAJATANG;;

    // Ranged
    case WPN_SLING:         return TILEP_HAND1_SLING;
    case WPN_BOW:           return TILEP_HAND1_BOW2;
    case WPN_CROSSBOW:      return TILEP_HAND1_CROSSBOW;
    case WPN_BLOWGUN:       return TILEP_HAND1_BLOWGUN;
    case WPN_LONGBOW:       return TILEP_HAND1_BOW3;

    default: return 0;
    }
}

int tilep_equ_armour(const item_def &item)
{
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (is_unrandom_artefact( item ))
    {
        const int tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile != -1)
            return tile;
    }

    switch (item.sub_type)
    {

    case ARM_ROBE:
        switch (item.colour)
        {
        // We've got a zillion robes; let's use 'em!
        case BLACK:       return TILEP_BODY_ROBE_BLACK_RED;
        case BLUE:        return TILEP_BODY_ROBE_BLUE;
        case LIGHTBLUE:   return TILEP_BODY_ROBE_BLUE_WHITE;
        case GREEN:       return TILEP_BODY_ROBE_GREEN;
        case LIGHTGREEN:  return TILEP_BODY_ROBE_BLUE_GREEN;
        case CYAN:        return TILEP_BODY_ROBE_WHITE_GREEN;
        case LIGHTCYAN:   return TILEP_BODY_ROBE_CYAN;
        case RED:         return TILEP_BODY_ROBE_RED;
        case LIGHTRED:    return TILEP_BODY_ROBE_RED_GOLD;
        case MAGENTA:     return TILEP_BODY_ROBE_MAGENTA;
        case LIGHTMAGENTA:return TILEP_BODY_ROBE_RED3;
        case BROWN:       return TILEP_BODY_ROBE_BROWN;
        case YELLOW:      return TILEP_BODY_ROBE_YELLOW;
        case LIGHTGREY:   return TILEP_BODY_ROBE_GRAY2;
        case DARKGREY:    return TILEP_BODY_GANDALF_G;
        case WHITE:       return TILEP_BODY_ROBE_WHITE;
        default:          return 0;
        }

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

int tilep_equ_shield(const item_def &item)
{
    if (you.equip[EQ_SHIELD] == -1)
        return 0;

    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (is_unrandom_artefact( item ))
    {
        const int tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile != -1)
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

int tilep_equ_cloak(const item_def &item)
{
    if (you.equip[EQ_CLOAK] == -1)
        return 0;

    if (item.base_type != OBJ_ARMOUR || item.sub_type != ARM_CLOAK)
        return 0;

    if (is_unrandom_artefact( item ))
    {
        const int tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile != -1)
            return tile;
    }

    switch (item.colour)
    {
         case BLACK:
         case BLUE:
         case LIGHTBLUE:   return TILEP_CLOAK_BLUE;
         case GREEN:
         case LIGHTGREEN:  return TILEP_CLOAK_GREEN;
         case CYAN:
         case LIGHTCYAN:   return TILEP_CLOAK_CYAN;
         case RED:
         case LIGHTRED:    return TILEP_CLOAK_RED;
         case MAGENTA:
         case LIGHTMAGENTA:return TILEP_CLOAK_MAGENTA;
         case BROWN:       return TILEP_CLOAK_LBROWN;
         case YELLOW:      return TILEP_CLOAK_YELLOW;
         case LIGHTGREY:   return TILEP_CLOAK_GRAY;
         case DARKGREY:    return TILEP_CLOAK_BLACK;
         case WHITE:       return TILEP_CLOAK_WHITE;
         default:          return 0;
    }
}

int tilep_equ_helm(const item_def &item)
{
    if (you.equip[EQ_HELMET] == -1)
        return 0;
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const int tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile != -1)
            return tile;

        // Although there shouldn't be any, just in case
        // unhandled artefacts fall through to defaults...
    }

    int etype = _get_etype(item);
    int helmet_desc = get_helmet_desc(item);
    switch (item.sub_type)
    {
        case ARM_CAP:
            switch (item.colour)
            {
                case BLACK:
                case BLUE:
                case LIGHTBLUE:
                    return TILEP_HELM_FEATHER_BLUE;
                case GREEN:
                case LIGHTGREEN:
                case CYAN:
                case LIGHTCYAN:
                    return TILEP_HELM_FEATHER_GREEN;
                case RED:
                case LIGHTRED:
                case MAGENTA:
                case LIGHTMAGENTA:
                    return TILEP_HELM_FEATHER_RED;
                case BROWN:
                case YELLOW:
                    return TILEP_HELM_FEATHER_YELLOW;
                case LIGHTGREY:
                case DARKGREY:
                case WHITE:
                    return TILEP_HELM_FEATHER_WHITE;
            }
            return 0;

        case ARM_WIZARD_HAT:
            switch (item.colour)
            {
                case MAGENTA:
                case LIGHTMAGENTA:
                case BLACK:
                    return TILEP_HELM_WIZARD_BLACKRED;
                case BLUE:
                case LIGHTBLUE:
                    return TILEP_HELM_WIZARD_BLUE;
                case GREEN:
                case LIGHTGREEN:
                    return TILEP_HELM_WIZARD_DARKGREEN;
                case CYAN:
                    return TILEP_HELM_WIZARD_PURPLE;
                case LIGHTCYAN:
                    return TILEP_HELM_WIZARD_BLUEGREEN;
                case RED:
                case LIGHTRED:
                    return TILEP_HELM_WIZARD_RED;
                case BROWN:
                    return TILEP_HELM_WIZARD_BROWN;
                case YELLOW:
                    return TILEP_HELM_WIZARD_BLACKGOLD;
                case LIGHTGREY:
                case DARKGREY:
                case WHITE:
                    return TILEP_HELM_WIZARD_WHITE;
            }
            return 0;

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
                            return TILEP_HELM_HELM_OFS; // urgh
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
                            return TILEP_HELM_FHELM_OFS; // urgh
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

int tilep_equ_gloves(const item_def &item)
{
    if (you.equip[EQ_GLOVES] == -1)
        return 0;
    if (item.base_type != OBJ_ARMOUR || item.sub_type != ARM_GLOVES)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const int tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile != -1)
            return tile;
    }

    switch (_get_etype(item))
    {
        default:
        case 0:
            switch (item.colour)
            {
                case LIGHTBLUE:
                    return TILEP_ARM_GLOVE_BLUE;
                default:
                case BROWN:
                    return TILEP_ARM_GLOVE_BROWN;
            }
        case 1:
        case 2:
        case 3:
        case 4:
            switch (item.colour)
            {
                case LIGHTBLUE:
                default:
                    return TILEP_ARM_GLOVE_CHUNLI;
                case BROWN:
                    return TILEP_ARM_GLOVE_GRAYFIST;
            }
    }

    return 0;
}

int tilep_equ_boots(const item_def &item)
{
    if (you.equip[EQ_BOOTS] == -1)
        return 0;
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    int etype = _get_etype(item);

    if (item.sub_type == ARM_NAGA_BARDING)
        return TILEP_BOOTS_NAGA_BARDING + std::min(etype, 3);

    if (item.sub_type == ARM_CENTAUR_BARDING)
        return TILEP_BOOTS_CENTAUR_BARDING + std::min(etype, 3);

    if (item.sub_type != ARM_BOOTS)
        return 0;

    if (is_unrandom_artefact(item))
    {
        const int tile = unrandart_to_doll_tile(find_unrandart_index(item));
        if (tile != -1)
            return tile;
    }

    switch (etype)
    {
        default:
        case 0:
            return TILEP_BOOTS_MIDDLE_BROWN3;
        case 1:
        case 2:
            switch (item.colour)
            {
                case BROWN:
                default:
                    return TILEP_BOOTS_MESH_RED;
                case BLUE:
                    return TILEP_BOOTS_MESH_BLUE;
            }
        case 3:
        case 4:
            switch (item.colour)
            {
                case BROWN:
                    return TILEP_BOOTS_LONG_RED;
                default:
                case BLUE:
                    return TILEP_BOOTS_BLUE_GOLD;
            }
    }

    return 0;
}

#ifdef TILEP_DEBUG
/*
 *   Debugging routines
 */

const char *get_ctg_name(int part)
{
    return tilep_parts_name[part];
}

int get_ctg_idx(char *name)
{
    int i;

    for (i = 0; i < TILEP_PART_MAX; i++)
        if (strcmp(name, tilep_parts_name[i]) == 0)
            return i;

    return 0;
}

const char *get_parts_name(int part, int idx)
{
    static char tmp[10];
    const char *ptr = tilep_comment[ tilep_comment_ofs[part] -1 + idx ];

    if (idx == 0)
        return "";

    if (ptr[0] == 0)
    {
        sprintf(tmp,"%02d",idx);
        return tmp;
    }
    return ptr;
}

int get_parts_idx(int part, char *name)
{
    int res = atoi(name);
    int i;

    for (i = 0; i < tilep_parts_total[part]; i++)
        if (strcmp(name, tilep_comment[ tilep_comment_ofs[part]+i]) == 0)
            return i+1;

    return res;
}
#endif /* TILEP_DEBUG */

/*
 * Tile data preparation
 */

// for clarity
enum SpecialIdx
{
    SPECIAL_N    = 0,
    SPECIAL_NE   = 1,
    SPECIAL_E    = 2,
    SPECIAL_SE   = 3,
    SPECIAL_S    = 4,
    SPECIAL_SW   = 5,
    SPECIAL_W    = 6,
    SPECIAL_NW   = 7,
    SPECIAL_FULL = 8
};

int jitter(SpecialIdx i)
{
    return (i + random_range(-1, 1) + 8) % 8;
}

void tile_clear_flavour()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        env.tile_flv(*ri).floor   = 0;
        env.tile_flv(*ri).wall    = 0;
        env.tile_flv(*ri).special = 0;
        env.tile_flv(*ri).feat    = 0;
    }
}

// For floors and walls that have not already been set to a particular tile,
// set them to a random instance of the default floor and wall tileset.
void tile_init_flavour()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        tile_init_flavour(*ri);
}

void tile_init_flavour(const coord_def &gc)
{
    if (!map_bounds(gc))
        return;

    if (!env.tile_flv(gc).floor)
    {
        int floor_base = env.tile_default.floor;
        int colour = env.grid_colours(gc);
        if (colour)
            floor_base = tile_dngn_coloured(floor_base, colour);
        env.tile_flv(gc).floor = _pick_random_dngn_tile(floor_base);
    }

    if (!env.tile_flv(gc).wall)
    {
        int wall_base = env.tile_default.wall;
        int colour = env.grid_colours(gc);
        if (colour)
            wall_base = tile_dngn_coloured(wall_base, colour);
        env.tile_flv(gc).wall = _pick_random_dngn_tile(wall_base);
    }

    if (feat_is_door(grd(gc)))
    {
        // Check for horizontal gates.

        const coord_def left(gc.x - 1, gc.y);
        const coord_def right(gc.x + 1, gc.y);

        bool door_left  = (grd(left) == grd(gc));
        bool door_right = (grd(right) == grd(gc));

        if (door_left || door_right)
        {
            int target;
            if (door_left && door_right)
                target = TILE_DNGN_GATE_CLOSED_MIDDLE;
            else if (door_left)
                target = TILE_DNGN_GATE_CLOSED_RIGHT;
            else
                target = TILE_DNGN_GATE_CLOSED_LEFT;

            // NOTE: This requires that closed gates and open gates
            // are positioned in the tile set relative to their
            // door counterpart.
            env.tile_flv(gc).special = target - TILE_DNGN_CLOSED_DOOR;
        }
        else
            env.tile_flv(gc).special = 0;
    }
    else if (feat_is_secret_door(grd(gc)))
        env.tile_flv(gc).special = 0;
    else if (!env.tile_flv(gc).special)
        env.tile_flv(gc).special = random2(256);
}

void tile_default_flv(level_area_type lev, branch_type br, tile_flavour &flv)
{
    flv.wall    = TILE_WALL_NORMAL;
    flv.floor   = TILE_FLOOR_NORMAL;
    flv.special = 0;

    if (lev == LEVEL_PANDEMONIUM)
    {
        flv.floor = TILE_FLOOR_TOMB;
        switch (random2(7))
        {
        default:
        case 0: flv.wall = TILE_WALL_ZOT_BLUE; break;
        case 1: flv.wall = TILE_WALL_ZOT_RED; break;
        case 2: flv.wall = TILE_WALL_ZOT_MAGENTA; break;
        case 3: flv.wall = TILE_WALL_ZOT_GREEN; break;
        case 4: flv.wall = TILE_WALL_ZOT_CYAN; break;
        case 5: flv.wall = TILE_WALL_ZOT_YELLOW; break;
        case 6: flv.wall = TILE_WALL_ZOT_WHITE; break;
        }

        if (one_chance_in(3))
            flv.wall = TILE_WALL_FLESH;
        if (one_chance_in(3))
            flv.floor = TILE_FLOOR_NERVES;

        return;
    }
    else if (lev == LEVEL_ABYSS)
    {
        flv.floor = TILE_FLOOR_NERVES;
        switch (random2(6))
        {
        default:
        case 0: flv.wall = TILE_WALL_HIVE; break;
        case 1: flv.wall = TILE_WALL_PEBBLE_RED; break;
        case 2: flv.wall = TILE_WALL_SLIME; break;
        case 3: flv.wall = TILE_WALL_ICE; break;
        case 4: flv.wall = TILE_WALL_HALL; break;
        case 5: flv.wall = TILE_WALL_UNDEAD; break;
        }
        return;
    }
    else if (lev == LEVEL_LABYRINTH)
    {
        flv.wall  = TILE_WALL_UNDEAD;
        flv.floor = TILE_FLOOR_TOMB;
        return;
    }
    else if (lev == LEVEL_PORTAL_VAULT)
    {
        // These should be handled in the respective lua files.
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_NORMAL;
        return;
    }

    switch (br)
    {
    case BRANCH_MAIN_DUNGEON:
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_NORMAL;
        return;

    case BRANCH_HIVE:
        flv.wall  = TILE_WALL_HIVE;
        flv.floor = TILE_FLOOR_HIVE;
        return;

    case BRANCH_VAULTS:
        flv.wall  = TILE_WALL_VAULT;
        flv.floor = TILE_FLOOR_VAULT;
        return;

    case BRANCH_ECUMENICAL_TEMPLE:
        flv.wall  = TILE_WALL_VINES;
        flv.floor = TILE_FLOOR_VINES;
        return;

    case BRANCH_ELVEN_HALLS:
    case BRANCH_HALL_OF_BLADES:
        flv.wall  = TILE_WALL_HALL;
        flv.floor = TILE_FLOOR_HALL;
        return;

    case BRANCH_TARTARUS:
    case BRANCH_CRYPT:
    case BRANCH_VESTIBULE_OF_HELL:
        flv.wall  = TILE_WALL_UNDEAD;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case BRANCH_TOMB:
        flv.wall  = TILE_WALL_TOMB;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case BRANCH_DIS:
        flv.wall  = TILE_WALL_ZOT_CYAN;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case BRANCH_GEHENNA:
        flv.wall  = TILE_WALL_ZOT_RED;
        flv.floor = TILE_FLOOR_ROUGH_RED;
        return;

    case BRANCH_COCYTUS:
        flv.wall  = TILE_WALL_ICE;
        flv.floor = TILE_FLOOR_ICE;
        return;

    case BRANCH_ORCISH_MINES:
        flv.wall  = TILE_WALL_ORC;
        flv.floor = TILE_FLOOR_ORC;
        return;

    case BRANCH_LAIR:
        flv.wall  = TILE_WALL_LAIR;
        flv.floor = TILE_FLOOR_LAIR;
        return;

    case BRANCH_SLIME_PITS:
        flv.wall  = TILE_WALL_SLIME;
        flv.floor = TILE_FLOOR_SLIME;
        return;

    case BRANCH_SNAKE_PIT:
        flv.wall  = TILE_WALL_SNAKE;
        flv.floor = TILE_FLOOR_SNAKE;
        return;

    case BRANCH_SWAMP:
        flv.wall  = TILE_WALL_SWAMP;
        flv.floor = TILE_FLOOR_SWAMP;
        return;

    case BRANCH_SHOALS:
        flv.wall  = TILE_WALL_YELLOW_ROCK;
        flv.floor = TILE_FLOOR_SAND_STONE;
        return;

    case BRANCH_HALL_OF_ZOT:
        flv.wall  = TILE_WALL_ZOT_YELLOW;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case NUM_BRANCHES:
        break;
    }
}

void tile_init_default_flavour()
{
    tile_default_flv(you.level_type, you.where_are_you, env.tile_default);
}

// FIXME: Needs to be updated whenever the order of clouds or monsters changes.
int get_clean_map_idx(int tile_idx)
{
    int idx = tile_idx & TILE_FLAG_MASK;
    if (idx >= TILE_CLOUD_FIRE_0 && idx <= TILE_CLOUD_TLOC_ENERGY
        || idx >= TILEP_MONS_PANDEMONIUM_DEMON && idx <= TILEP_MONS_TEST_SPAWNER
        || idx >= TILEP_MCACHE_START)
    {
        return 0;
    }

    return tile_idx;
}

static bool _adjacent_target(dungeon_feature_type target, int x, int y)
{
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
        {
            if (!map_bounds(x+i, y+j))
                continue;
            if (grd[x+i][y+j] == target)
                return (true);
        }

    return (false);
}

void tile_floor_halo(dungeon_feature_type target, int tile)
{
    for (int x = 0; x < GXM; x++)
    {
        for (int y = 0; y < GYM; y++)
        {
            if (grd[x][y] < DNGN_FLOOR_MIN)
                continue;
            if (!_adjacent_target(target, x, y))
                continue;

            bool l_flr = (x > 0) ? grd[x-1][y] >= DNGN_FLOOR_MIN : false;
            bool r_flr = (x < GXM - 1) ? grd[x+1][y] >= DNGN_FLOOR_MIN : false;
            bool u_flr = (y > 0) ? grd[x][y-1] >= DNGN_FLOOR_MIN : false;
            bool d_flr = (y < GYM - 1) ? grd[x][y+1] >= DNGN_FLOOR_MIN : false;

            bool l_target = _adjacent_target(target, x-1, y);
            bool r_target = _adjacent_target(target, x+1, y);
            bool u_target = _adjacent_target(target, x, y-1);
            bool d_target = _adjacent_target(target, x, y+1);

            // The special tiles contains part floor and part special, so
            // if there are adjacent floor or special tiles, we should
            // do our best to "connect" them appropriately.  If there are
            // are other tiles there (walls, doors, whatever...) then it
            // doesn't matter.
            bool l_nrm = (l_flr && !l_target);
            bool r_nrm = (r_flr && !r_target);
            bool u_nrm = (u_flr && !u_target);
            bool d_nrm = (d_flr && !d_target);

            bool l_spc = (l_flr && l_target);
            bool r_spc = (r_flr && r_target);
            bool u_spc = (u_flr && u_target);
            bool d_spc = (d_flr && d_target);

            if (l_nrm && r_nrm || u_nrm && d_nrm)
            {
                // Not much to do here...
                env.tile_flv[x][y].floor = tile + SPECIAL_FULL;
            }
            else if (l_nrm)
            {
                if (u_nrm)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NW;
                else if (d_nrm)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SW;
                else if (u_spc && d_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_W;
                else if (u_spc && r_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SW;
                else if (d_spc && r_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NW;
                else if (u_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_W : SPECIAL_SW);
                }
                else if (d_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_W : SPECIAL_NW);
                }
                else
                    env.tile_flv[x][y].floor = tile + jitter(SPECIAL_W);
            }
            else if (r_nrm)
            {
                if (u_nrm)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                else if (d_nrm)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                else if (u_spc && d_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_E;
                else if (u_spc && l_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                else if (d_spc && l_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                else if (u_spc)
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_E : SPECIAL_SE);
                else if (d_spc)
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_E : SPECIAL_NE);
                else
                    env.tile_flv[x][y].floor = tile + jitter(SPECIAL_E);
            }
            else if (u_nrm)
            {
                if (r_spc && l_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_N;
                else if (r_spc && d_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NW;
                else if (l_spc && d_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                else if (r_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_N : SPECIAL_NW);
                }
                else if (l_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_N : SPECIAL_NE);
                }
                else
                    env.tile_flv[x][y].floor = tile + jitter(SPECIAL_N);
            }
            else if (d_nrm)
            {
                if (r_spc && l_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_S;
                else if (r_spc && u_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SW;
                else if (l_spc && u_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                else if (r_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_S : SPECIAL_SW);
                }
                else if (l_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_S : SPECIAL_SE);
                }
                else
                    env.tile_flv[x][y].floor = tile + jitter(SPECIAL_S);
            }
            else if (u_spc && d_spc)
            {
                // We know this value is already initialised and
                // is necessarily in bounds.
                int t = env.tile_flv[x][y-1].floor - tile;
                if (t == SPECIAL_NE || t == SPECIAL_E)
                    env.tile_flv[x][y].floor = tile + SPECIAL_E;
                else if (t == SPECIAL_NW || t == SPECIAL_W)
                    env.tile_flv[x][y].floor = tile + SPECIAL_W;
                else
                    env.tile_flv[x][y].floor = tile + SPECIAL_FULL;
            }
            else if (r_spc && l_spc)
            {
                // We know this value is already initialised and
                // is necessarily in bounds.
                int t = env.tile_flv[x-1][y].floor - tile;
                if (t == SPECIAL_NW || t == SPECIAL_N)
                    env.tile_flv[x][y].floor = tile + SPECIAL_N;
                else if (t == SPECIAL_SW || t == SPECIAL_S)
                    env.tile_flv[x][y].floor = tile + SPECIAL_S;
                else
                    env.tile_flv[x][y].floor = tile + SPECIAL_FULL;
            }
            else if (u_spc && l_spc)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_SE;
            }
            else if (u_spc && r_spc)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_SW;
            }
            else if (d_spc && l_spc)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_NE;
            }
            else if (d_spc && r_spc)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_NW;
            }
            else
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_FULL;
            }
        }
    }

    // Second pass for clean up.  The only bad part about the above
    // algorithm is that it could turn a block of floor like this:
    //
    // N4NN
    // 3125
    // NN6N
    //
    // (KEY: N = normal floor, # = special floor)
    //
    // Into these flavours:
    // 1 - SPECIAL_S
    // 2 - SPECIAL_N
    // 3-6, not important
    //
    // Generally the tiles don't fit with a north to the right or left
    // of a south tile.  What we really want to do is to separate the
    // two regions, by making 1 a SPECIAL_SE and 2 a SPECIAL_NW tile.
    for (int y = 0; y < GYM - 1; ++y)
        for (int x = 0; x < GXM - 1; ++x)
        {
            int this_spc = env.tile_flv[x][y].floor - tile;
            if (this_spc < 0 || this_spc > 8)
                continue;

            if (this_spc != SPECIAL_N && this_spc != SPECIAL_S
                && this_spc != SPECIAL_E && this_spc != SPECIAL_W)
            {
                continue;
            }

            int right_spc = x < GXM - 1 ? env.tile_flv[x+1][y].floor - tile
                                        : SPECIAL_FULL;
            int down_spc  = y < GYM - 1 ? env.tile_flv[x][y+1].floor - tile
                                        : SPECIAL_FULL;

            if (this_spc == SPECIAL_N && right_spc == SPECIAL_S)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                env.tile_flv[x+1][y].floor = tile + SPECIAL_SW;
            }
            else if (this_spc == SPECIAL_S && right_spc == SPECIAL_N)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                env.tile_flv[x+1][y].floor = tile + SPECIAL_NW;
            }
            else if (this_spc == SPECIAL_E && down_spc == SPECIAL_W)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                env.tile_flv[x][y+1].floor = tile + SPECIAL_NW;
            }
            else if (this_spc == SPECIAL_W && down_spc == SPECIAL_E)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                env.tile_flv[x][y+1].floor = tile + SPECIAL_SW;
            }
        }
}

// Called from item() in view.cc
void tile_place_item(int x, int y, int idx)
{
    ASSERT(idx != NON_ITEM);

    int t = tileidx_item(mitm[idx]);
    if (mitm[idx].link != NON_ITEM)
        t |= TILE_FLAG_S_UNDER;

    env.tile_fg[x][y] = t;

    if (item_needs_autopickup(mitm[idx]))
        env.tile_bg[x][y] |= TILE_FLAG_CURSOR3;
}

// Called from item() in view.cc
void tile_place_item_marker(int x, int y, int idx)
{
    ASSERT(idx != NON_ITEM);

    env.tile_fg[x][y] |= TILE_FLAG_S_UNDER;

    if (item_needs_autopickup(mitm[idx]))
        env.tile_bg[x][y] |= TILE_FLAG_CURSOR3;
}

// Called from show_def::_update_monster() in show.cc
void tile_place_monster(int gx, int gy, int idx, bool foreground, bool detected)
{
    if (idx == NON_MONSTER)
        return;

    const coord_def gc(gx, gy);
    const coord_def ep = view2show(grid2view(gc));

    int t    = _tileidx_monster(idx, detected);
    int t0   = t & TILE_FLAG_MASK;
    int flag = t & (~TILE_FLAG_MASK);

    if (mons_is_mimic(menv[idx].type))
    {
        const monsters *mon = &menv[idx];
        if (!mons_is_known_mimic(mon))
        {
            // If necessary add item brand.
            if (you.visible_igrd(gc) != NON_ITEM)
            {
                if (foreground)
                    t |= TILE_FLAG_S_UNDER;
                else
                    t0 |= TILE_FLAG_S_UNDER;
            }

            if (item_needs_autopickup(get_mimic_item(mon)))
            {
                if (foreground)
                    env.tile_bg[ep.x][ep.y] |= TILE_FLAG_CURSOR3;
                else
                    env.tile_bk_bg(gc) |= TILE_FLAG_CURSOR3;
            }
        }
    }
    else if (mons_is_stationary(&menv[idx]))
    {
        // If necessary add item brand.
        if (you.visible_igrd(gc) != NON_ITEM)
        {
            if (foreground)
                t |= TILE_FLAG_S_UNDER;
            else
                t0 |= TILE_FLAG_S_UNDER;
        }
    }
    else
    {
        unsigned int mcache_idx = mcache.register_monster(&menv[idx]);
        t = flag | (mcache_idx ? mcache_idx : t0);
    }

    if (foreground)
    {
        // Add name tags.
        env.tile_fg[ep.x][ep.y] = t;
        const monsters *mon = &menv[idx];

        if (!mon->visible_to(&you)
            || mons_is_lurking(mon)
            || mons_is_unknown_mimic(mon)
            || mons_class_flag(mon->type, M_NO_EXP_GAIN))
        {
            return;
        }

        const tag_pref pref = Options.tile_tag_pref;
        if (pref == TAGPREF_NONE)
            return;
        else if (pref == TAGPREF_TUTORIAL)
        {
            const long kills = you.kills->num_kills(mon);
            const int limit  = 0;

            if (!mon->is_named() && kills > limit)
                return;
        }
        else if (!mon->is_named())
            return;

        if (pref != TAGPREF_NAMED && mon->friendly())
            return;

        // HACK.  Names cover up pan demons in a weird way.
        if (mon->type == MONS_PANDEMONIUM_DEMON)
            return;

        tiles.add_text_tag(TAG_NAMED_MONSTER, mon);
    }
    else
    {
        // Retain the magic mapped terrain, but don't give away the real
        // features.
        if (is_terrain_mapped(gc))
        {
            unsigned int fg;
            unsigned int bg;
            tileidx_unseen(fg, bg, get_feat_symbol(grd(gc)), gc);
            env.tile_bk_bg(gc) = bg;
//             env.tile_bk_bg(gc) = fg;
        }
        env.tile_bk_fg(gc) = t0;
    }
}

void tile_place_cloud(int x, int y, cloud_struct cl)
{
    // In the Shoals, ink is handled differently. (jpeg)
    // I'm not sure it is even possible anywhere else, but just to be safe...
    if (cl.type != CLOUD_INK || !player_in_branch(BRANCH_SHOALS))
        env.tile_fg[x][y] = _tileidx_cloud(cl);
}

unsigned int num_tile_rays = 0;
struct tile_ray
{
    coord_def ep;
    bool in_range;
};
FixedVector<tile_ray, 30> tile_ray_vec;

void tile_place_ray(const coord_def& gc, bool in_range)
{
    // Record rays for later.  The curses version just applies
    // rays directly to the screen.  The tiles version doesn't have
    // (nor want) such direct access.  So, it batches up all of the
    // rays and applies them in viewwindow(...).
    if (num_tile_rays < tile_ray_vec.size() - 1)
    {
        tile_ray_vec[num_tile_rays].in_range = in_range;
        tile_ray_vec[num_tile_rays++].ep = view2show(grid2view(gc));
    }
}

void tile_draw_rays(bool reset_count)
{
    for (unsigned int i = 0; i < num_tile_rays; i++)
    {
        int flag = tile_ray_vec[i].in_range ? TILE_FLAG_RAY : TILE_FLAG_RAY_OOR;
        env.tile_bg[tile_ray_vec[i].ep.x][tile_ray_vec[i].ep.y] |= flag;
    }

    if (reset_count)
        num_tile_rays = 0;
}

static bool _suppress_blood(const coord_def pos)
{
    if (!you.see_cell(pos))
        return (true);

    const dungeon_feature_type feat = grd(pos);
    if (feat == DNGN_TREE)
        return (true);

    if (feat >= DNGN_FOUNTAIN_BLUE && feat <= DNGN_PERMADRY_FOUNTAIN)
        return (true);

    if (feat_is_altar(feat))
        return (true);

    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return (true);

    const trap_def *trap = find_trap(pos);
    if (trap && trap->type == TRAP_SHAFT && trap->is_known())
        return (true);

    return (false);
}

void tile_apply_properties(const coord_def &gc, screen_buffer_t *fg,
                           screen_buffer_t *bg)
{
    if (is_excluded(gc))
    {
        if (is_exclude_root(gc))
            *bg |= TILE_FLAG_EXCL_CTR;
        else
            *bg |= TILE_FLAG_TRAV_EXCL;
    }

    if (!map_bounds(gc))
        return;

    _apply_variations(env.tile_flv(gc), bg);

    bool print_blood = true;
    if (haloed(gc))
    {
        monsters *mon = monster_at(gc);
        if (you.see_cell(gc) && mon)
        {
            if (!mons_class_flag(mon->type, M_NO_EXP_GAIN)
                 && (!mons_is_mimic(mon->type)
                     || testbits(mon->flags, MF_KNOWN_MIMIC)))
            {
                *bg |= TILE_FLAG_HALO;
                print_blood = false;
            }
        }
    }

    if (print_blood && _suppress_blood(gc))
        print_blood = false;

    // Mold has the same restrictions as blood
    // but mold takes precendence over blood.
    if (print_blood)
    {
        if (is_moldy(gc))
            *bg |= TILE_FLAG_MOLD;
        else if (is_bloodcovered(gc))
            *bg |= TILE_FLAG_BLOOD;
    }

    const dungeon_feature_type feat = grd(gc);
    if (feat_is_water(feat) || feat == DNGN_LAVA)
        *bg |= TILE_FLAG_WATER;

    if (is_sanctuary(gc))
        *bg |= TILE_FLAG_SANCTUARY;

    if (silenced(gc))
        *bg |= TILE_FLAG_SILENCED;
}

static FixedVector<const char*, TAGPREF_MAX>
    tag_prefs("none", "tutorial", "named", "enemy");

tag_pref string2tag_pref(const char *opt)
{
    for (int i = 0; i < TAGPREF_MAX; i++)
    {
        if (!stricmp(opt, tag_prefs[i]))
            return ((tag_pref)i);
    }

    return TAGPREF_ENEMY;
}

const char *tag_pref2string(tag_pref pref)
{
    return tag_prefs[pref];
}

#endif

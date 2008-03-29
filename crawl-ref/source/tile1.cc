/*
 *  File:       tile1.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */

#include "AppHdr.h"
#ifdef USE_TILE
#include <stdio.h>
#include "decks.h"
#include "direct.h"
#include "externs.h"
#include "food.h"
#include "itemname.h"
#include "items.h"
#include "itemprop.h"
#include "macro.h"
#include "monstuff.h"
#include "mon-util.h"
#include "player.h"
#include "randart.h"
#include "stuff.h"
#include "terrain.h"
#include "tiles.h"
#include "tiledef-p.h"
#include "traps.h"
#include "travel.h"
#include "view.h"

// tile index cache to reduce tileidx() calls
static FixedArray < unsigned int, GXM, GYM > tile_dngn;

bool is_bazaar()
{
    return (you.level_type == LEVEL_PORTAL_VAULT && 
            you.level_type_name == "bazaar");
}

unsigned short get_bazaar_special_colour()
{
    return YELLOW;
}

void TileNewLevel(bool first_time)
{
    GmapInit(false);
    TileLoadWall(false);
    tile_clear_buf();
    if (first_time)
        tile_init_flavor();
}

/**** tile index routines ****/

int tile_unseen_flag(const coord_def& gc)
{
    if (!map_bounds(gc))
        return TILE_FLAG_UNSEEN;
    else if (is_terrain_known(gc.x,gc.y)
                && !is_terrain_seen(gc.x,gc.y)
             || is_envmap_detected_item(gc.x,gc.y)
             || is_envmap_detected_mons(gc.x,gc.y))
    {
        return TILE_FLAG_MM_UNSEEN;
    }
    else
    {
        return TILE_FLAG_UNSEEN;
    }
}

static int _tileidx_monster_base(int mon_idx, bool detected)
{
    const monsters* mon = &menv[mon_idx];
    int grid = grd[mon->x][mon->y];
    bool in_water = (grid == DNGN_SHALLOW_WATER || grid == DNGN_DEEP_WATER);
    
    int type = mon->type;
    
    // show only base class for detected monsters
    if (detected)
        type = mons_genus(type);

    switch (type)
    {
    case MONS_GIANT_ANT:
        return TILE_MONS_GIANT_ANT;
    case MONS_GIANT_BAT:
        return TILE_MONS_GIANT_BAT;
    case MONS_CENTAUR:
        return TILE_MONS_CENTAUR;
    case MONS_RED_DEVIL:
        return TILE_MONS_RED_DEVIL;
    case MONS_ETTIN:
        return TILE_MONS_ETTIN;
    case MONS_FUNGUS:
        return TILE_MONS_FUNGUS;
    case MONS_GOBLIN:
        return TILE_MONS_GOBLIN;
    case MONS_HOUND:
        return TILE_MONS_HOUND;
    case MONS_IMP:
        return TILE_MONS_IMP;
    case MONS_JACKAL:
        return TILE_MONS_JACKAL;
    case MONS_KILLER_BEE:
        return TILE_MONS_KILLER_BEE;
    case MONS_KILLER_BEE_LARVA:
        return TILE_MONS_KILLER_BEE_LARVA;
    case MONS_MANTICORE:
        return TILE_MONS_MANTICORE;
    case MONS_NECROPHAGE:
        return TILE_MONS_NECROPHAGE;
    case MONS_ORC:
        return TILE_MONS_ORC;
    case MONS_PHANTOM:
        return TILE_MONS_PHANTOM;
    case MONS_QUASIT:
        return TILE_MONS_QUASIT;
    case MONS_RAT:
        return TILE_MONS_RAT;
    case MONS_SCORPION:
        return TILE_MONS_SCORPION;
    case MONS_UGLY_THING :
        return TILE_MONS_UGLY_THING ;
    case MONS_FIRE_VORTEX:
        return TILE_MONS_FIRE_VORTEX;
    case MONS_WORM:
        return TILE_MONS_WORM;
    case MONS_ABOMINATION_SMALL:
        return TILE_MONS_ABOMINATION_SMALL;
    case MONS_YELLOW_WASP:
        return TILE_MONS_YELLOW_WASP;
    case MONS_ZOMBIE_SMALL:
        return TILE_MONS_ZOMBIE_SMALL;
    case MONS_ANGEL:
        return TILE_MONS_ANGEL;
    case MONS_GIANT_BEETLE:
        return TILE_MONS_GIANT_BEETLE;
    case MONS_CYCLOPS:
        return TILE_MONS_CYCLOPS;
    case MONS_DRAGON:
        return TILE_MONS_DRAGON;
    case MONS_TWO_HEADED_OGRE:
        return TILE_MONS_TWO_HEADED_OGRE;
    case MONS_FIEND:
        return TILE_MONS_FIEND;
    case MONS_GIANT_SPORE:
        return TILE_MONS_GIANT_SPORE;
    case MONS_HOBGOBLIN:
        return TILE_MONS_HOBGOBLIN;
    case MONS_ICE_BEAST:
        return TILE_MONS_ICE_BEAST;
    case MONS_JELLY:
        return TILE_MONS_JELLY;
    case MONS_KOBOLD:
        return TILE_MONS_KOBOLD;
    case MONS_LICH:
        return TILE_MONS_LICH;
    case MONS_MUMMY:
        return TILE_MONS_MUMMY;
    case MONS_GUARDIAN_NAGA:
        return TILE_MONS_GUARDIAN_NAGA;
    case MONS_OGRE:
        return TILE_MONS_OGRE;
    case MONS_PLANT:
        return TILE_MONS_PLANT;
    case MONS_QUEEN_BEE:
        return TILE_MONS_QUEEN_BEE;
    case MONS_RAKSHASA:
        return TILE_MONS_RAKSHASA;
    case MONS_SNAKE:
        return TILE_MONS_SNAKE;
    case MONS_TROLL:
        return TILE_MONS_TROLL;
    case MONS_UNSEEN_HORROR:
        return TILE_MONS_UNSEEN_HORROR;
    case MONS_VAMPIRE:
        return TILE_MONS_VAMPIRE;
    case MONS_WRAITH:
        return TILE_MONS_WRAITH;
    case MONS_ABOMINATION_LARGE:
        return TILE_MONS_ABOMINATION_LARGE + ((mon->colour)%7);
    case MONS_YAK:
        return TILE_MONS_YAK;
    case MONS_ZOMBIE_LARGE:
        return TILE_MONS_ZOMBIE_LARGE;
    case MONS_ORC_WARRIOR:
        return TILE_MONS_ORC_WARRIOR;
    case MONS_KOBOLD_DEMONOLOGIST:
        return TILE_MONS_KOBOLD_DEMONOLOGIST;
    case MONS_ORC_WIZARD:
        return TILE_MONS_ORC_WIZARD;
    case MONS_ORC_KNIGHT:
        return TILE_MONS_ORC_KNIGHT;
    case MONS_WYVERN:
        return TILE_MONS_WYVERN;
    case MONS_BIG_KOBOLD:
        return TILE_MONS_BIG_KOBOLD;
    case MONS_GIANT_EYEBALL:
        return TILE_MONS_GIANT_EYEBALL;
    case MONS_WIGHT:
        return TILE_MONS_WIGHT;
    case MONS_OKLOB_PLANT:
        return TILE_MONS_OKLOB_PLANT;
    case MONS_WOLF_SPIDER:
        return TILE_MONS_WOLF_SPIDER;
    case MONS_SHADOW:
        return TILE_MONS_SHADOW;
    case MONS_HUNGRY_GHOST:
        return TILE_MONS_HUNGRY_GHOST;
    case MONS_EYE_OF_DRAINING:
        return TILE_MONS_EYE_OF_DRAINING;
    case MONS_BUTTERFLY:
        return TILE_MONS_BUTTERFLY + ((mon->colour)%7);
    case MONS_WANDERING_MUSHROOM:
        return TILE_MONS_WANDERING_MUSHROOM;
    case MONS_EFREET:
        return TILE_MONS_EFREET;
    case MONS_BRAIN_WORM:
        return TILE_MONS_BRAIN_WORM;
    case MONS_GIANT_ORANGE_BRAIN:
        return TILE_MONS_GIANT_ORANGE_BRAIN;
    case MONS_BOULDER_BEETLE:
        return TILE_MONS_BOULDER_BEETLE;
    case MONS_FLYING_SKULL:
        return TILE_MONS_FLYING_SKULL;
    case MONS_HELL_HOUND:
        return TILE_MONS_HELL_HOUND;
    case MONS_MINOTAUR:
        return TILE_MONS_MINOTAUR;
    case MONS_ICE_DRAGON:
        return TILE_MONS_ICE_DRAGON;
    case MONS_SLIME_CREATURE:
        return TILE_MONS_SLIME_CREATURE;
    case MONS_FREEZING_WRAITH:
        return TILE_MONS_FREEZING_WRAITH;
    case MONS_RAKSHASA_FAKE:
        return TILE_MONS_RAKSHASA_FAKE;
    case MONS_GREAT_ORB_OF_EYES:
        return TILE_MONS_GREAT_ORB_OF_EYES;
    case MONS_HELLION:
        return TILE_MONS_HELLION;
    case MONS_ROTTING_DEVIL:
        return TILE_MONS_ROTTING_DEVIL;
    case MONS_TORMENTOR:
        return TILE_MONS_TORMENTOR;
    case MONS_REAPER:
        return TILE_MONS_REAPER;
    case MONS_SOUL_EATER:
        return TILE_MONS_SOUL_EATER;
    case MONS_HAIRY_DEVIL:
        return TILE_MONS_HAIRY_DEVIL;
    case MONS_ICE_DEVIL:
        return TILE_MONS_ICE_DEVIL;
    case MONS_BLUE_DEVIL:
        return TILE_MONS_BLUE_DEVIL;
    case MONS_BEAST:
        return TILE_MONS_BEAST;
    case MONS_IRON_DEVIL:
        return TILE_MONS_IRON_DEVIL;
    case MONS_GLOWING_SHAPESHIFTER:
        return TILE_MONS_GLOWING_SHAPESHIFTER;
    case MONS_SHAPESHIFTER:
        return TILE_MONS_SHAPESHIFTER;
    case MONS_GIANT_MITE:
        return TILE_MONS_GIANT_MITE;
    case MONS_STEAM_DRAGON:
        return TILE_MONS_STEAM_DRAGON;
    case MONS_VERY_UGLY_THING:
        return TILE_MONS_VERY_UGLY_THING;
    case MONS_ORC_SORCERER:
        return TILE_MONS_ORC_SORCERER;
    case MONS_HIPPOGRIFF:
        return TILE_MONS_HIPPOGRIFF;
    case MONS_GRIFFON:
        return TILE_MONS_GRIFFON;

    case MONS_HYDRA:
    {
      // Number of heads
      int  heads = mon->number;
      if (heads > 7) heads = 7;
      return TILE_MONS_HYDRA + heads - 1;
    }

    case MONS_SKELETON_SMALL:
        return TILE_MONS_SKELETON_SMALL;
    case MONS_SKELETON_LARGE:
        return TILE_MONS_SKELETON_LARGE;
    case MONS_HELL_KNIGHT:
        return TILE_MONS_HELL_KNIGHT;
    case MONS_NECROMANCER:
        return TILE_MONS_NECROMANCER;
    case MONS_WIZARD:
        return TILE_MONS_WIZARD;
    case MONS_ORC_PRIEST:
        return TILE_MONS_ORC_PRIEST;
    case MONS_ORC_HIGH_PRIEST:
        return TILE_MONS_ORC_HIGH_PRIEST;
    case MONS_HUMAN:
        return TILE_MONS_HUMAN;
    case MONS_GNOLL:
        return TILE_MONS_GNOLL;
    case MONS_CLAY_GOLEM:
        return TILE_MONS_CLAY_GOLEM;
    case MONS_WOOD_GOLEM:
        return TILE_MONS_WOOD_GOLEM;
    case MONS_STONE_GOLEM:
        return TILE_MONS_STONE_GOLEM;
    case MONS_IRON_GOLEM:
        return TILE_MONS_IRON_GOLEM;
    case MONS_CRYSTAL_GOLEM:
        return TILE_MONS_CRYSTAL_GOLEM;
    case MONS_TOENAIL_GOLEM:
        return TILE_MONS_TOENAIL_GOLEM;
    case MONS_MOTTLED_DRAGON:
        return TILE_MONS_MOTTLED_DRAGON;
    case MONS_EARTH_ELEMENTAL:
        return TILE_MONS_EARTH_ELEMENTAL;
    case MONS_FIRE_ELEMENTAL:
        return TILE_MONS_FIRE_ELEMENTAL;
    case MONS_AIR_ELEMENTAL:
        return TILE_MONS_AIR_ELEMENTAL;
    case MONS_ICE_FIEND:
        return TILE_MONS_ICE_FIEND;
    case MONS_SHADOW_FIEND:
        return TILE_MONS_SHADOW_FIEND;
    case MONS_BROWN_SNAKE:
        return TILE_MONS_BROWN_SNAKE;
    case MONS_GIANT_LIZARD:
        return TILE_MONS_GIANT_LIZARD;
    case MONS_SPECTRAL_WARRIOR:
        return TILE_MONS_SPECTRAL_WARRIOR;
    case MONS_PULSATING_LUMP:
        return TILE_MONS_PULSATING_LUMP;
    case MONS_STORM_DRAGON:
        return TILE_MONS_STORM_DRAGON;
    case MONS_YAKTAUR:
        return TILE_MONS_YAKTAUR;
    case MONS_DEATH_YAK:
        return TILE_MONS_DEATH_YAK;
    case MONS_ROCK_TROLL:
        return TILE_MONS_ROCK_TROLL;
    case MONS_STONE_GIANT:
        return TILE_MONS_STONE_GIANT;
    case MONS_FLAYED_GHOST:
        return TILE_MONS_FLAYED_GHOST;
    case MONS_BUMBLEBEE:
        return TILE_MONS_BUMBLEBEE;
    case MONS_REDBACK:
        return TILE_MONS_REDBACK;
    case MONS_INSUBSTANTIAL_WISP:
        return TILE_MONS_INSUBSTANTIAL_WISP;
    case MONS_VAPOUR:
        return TILE_MONS_VAPOUR;
    case MONS_OGRE_MAGE:
        return TILE_MONS_OGRE_MAGE;
    case MONS_SPINY_WORM:
        return TILE_MONS_SPINY_WORM;

    case MONS_DANCING_WEAPON:
    {
        // Use item tile
        item_def item = mitm[menv[mon_idx].inv[MSLOT_WEAPON]];
        return tileidx_item(item);
    }

    case MONS_TITAN:
        return TILE_MONS_TITAN;
    case MONS_GOLDEN_DRAGON:
        return TILE_MONS_GOLDEN_DRAGON;
    case MONS_ELF:
        return TILE_MONS_ELF;
    case MONS_LINDWURM:
        return TILE_MONS_LINDWURM;
    case MONS_ELEPHANT_SLUG:
        return TILE_MONS_ELEPHANT_SLUG;
    case MONS_WAR_DOG:
        return TILE_MONS_WAR_DOG;
    case MONS_GREY_RAT:
        return TILE_MONS_GREY_RAT;
    case MONS_GREEN_RAT:
        return TILE_MONS_GREEN_RAT;
    case MONS_ORANGE_RAT:
        return TILE_MONS_ORANGE_RAT;
    case MONS_BLACK_SNAKE:
        return TILE_MONS_BLACK_SNAKE;
    case MONS_SHEEP:
        return TILE_MONS_SHEEP;
    case MONS_GHOUL:
        return TILE_MONS_GHOUL;
    case MONS_HOG:
        return TILE_MONS_HOG;
    case MONS_GIANT_MOSQUITO:
        return TILE_MONS_GIANT_MOSQUITO;
    case MONS_GIANT_CENTIPEDE:
        return TILE_MONS_GIANT_CENTIPEDE;
    case MONS_IRON_TROLL:
        return TILE_MONS_IRON_TROLL;
    case MONS_NAGA:
        return TILE_MONS_NAGA;
    case MONS_FIRE_GIANT:
        return TILE_MONS_FIRE_GIANT;
    case MONS_FROST_GIANT:
        return TILE_MONS_FROST_GIANT;
    case MONS_FIREDRAKE:
        return TILE_MONS_FIREDRAKE;
    case MONS_SHADOW_DRAGON:
        return TILE_MONS_SHADOW_DRAGON;
    case MONS_YELLOW_SNAKE:
        return TILE_MONS_YELLOW_SNAKE;
    case MONS_GREY_SNAKE:
        return TILE_MONS_GREY_SNAKE;
    case MONS_DEEP_TROLL:
        return TILE_MONS_DEEP_TROLL;
    case MONS_GIANT_BLOWFLY:
        return TILE_MONS_GIANT_BLOWFLY;
    case MONS_RED_WASP:
        return TILE_MONS_RED_WASP;
    case MONS_SWAMP_DRAGON:
        return TILE_MONS_SWAMP_DRAGON;
    case MONS_SWAMP_DRAKE:
        return TILE_MONS_SWAMP_DRAKE;
    case MONS_SOLDIER_ANT:
        return TILE_MONS_SOLDIER_ANT;
    case MONS_HILL_GIANT:
        return TILE_MONS_HILL_GIANT;
    case MONS_QUEEN_ANT:
        return TILE_MONS_QUEEN_ANT;
    case MONS_ANT_LARVA:
        return TILE_MONS_ANT_LARVA;
    case MONS_GIANT_FROG:
        return TILE_MONS_GIANT_FROG;
    case MONS_GIANT_BROWN_FROG:
        return TILE_MONS_GIANT_BROWN_FROG;
    case MONS_SPINY_FROG:
        return TILE_MONS_SPINY_FROG;
    case MONS_BLINK_FROG:
        return TILE_MONS_BLINK_FROG;
    case MONS_GIANT_COCKROACH:
        return TILE_MONS_GIANT_COCKROACH;
    case MONS_SMALL_SNAKE:
        return TILE_MONS_SMALL_SNAKE;
    case MONS_SHUGGOTH:
        return TILE_TODO;
    case MONS_WOLF:
        return TILE_MONS_WOLF;
    case MONS_WARG:
        return TILE_MONS_WARG;
    case MONS_BEAR:
        return TILE_MONS_BEAR;
    case MONS_GRIZZLY_BEAR:
        return TILE_MONS_GRIZZLY_BEAR;
    case MONS_POLAR_BEAR:
        return TILE_MONS_POLAR_BEAR;
    case MONS_BLACK_BEAR:
        return TILE_MONS_BLACK_BEAR;
    case MONS_SIMULACRUM_SMALL:
        return TILE_MONS_SIMULACRUM_SMALL;
    case MONS_SIMULACRUM_LARGE:
        return TILE_MONS_SIMULACRUM_LARGE;
    case MONS_WHITE_IMP:
        return TILE_MONS_WHITE_IMP;
    case MONS_LEMURE:
        return TILE_MONS_LEMURE;
    case MONS_UFETUBUS:
        return TILE_MONS_UFETUBUS;
    case MONS_MANES:
        return TILE_MONS_MANES;
    case MONS_MIDGE:
        return TILE_MONS_MIDGE;
    case MONS_NEQOXEC:
        return TILE_MONS_NEQOXEC;
    case MONS_ORANGE_DEMON:
        return TILE_MONS_ORANGE_DEMON;
    case MONS_HELLWING:
        return TILE_MONS_HELLWING;
    case MONS_SMOKE_DEMON:
        return TILE_MONS_SMOKE_DEMON;
    case MONS_YNOXINUL:
        return TILE_MONS_YNOXINUL;
    case MONS_EXECUTIONER:
        return TILE_MONS_EXECUTIONER;
    case MONS_GREEN_DEATH:
        return TILE_MONS_GREEN_DEATH;
    case MONS_BLUE_DEATH:
        return TILE_MONS_BLUE_DEATH;
    case MONS_BALRUG:
        return TILE_MONS_BALRUG;
    case MONS_CACODEMON:
        return TILE_MONS_CACODEMON;
    case MONS_DEMONIC_CRAWLER:
        return TILE_MONS_DEMONIC_CRAWLER;
    case MONS_SUN_DEMON:
        return TILE_MONS_SUN_DEMON;
    case MONS_SHADOW_IMP:
        return TILE_MONS_SHADOW_IMP;
    case MONS_SHADOW_DEMON:
        return TILE_MONS_SHADOW_DEMON;
    case MONS_LOROCYPROCA:
        return TILE_MONS_LOROCYPROCA;
    case MONS_SHADOW_WRAITH:
        return TILE_MONS_SHADOW_WRAITH;
    case MONS_GIANT_AMOEBA:
        return TILE_MONS_GIANT_AMOEBA;
    case MONS_GIANT_SLUG:
        return TILE_MONS_GIANT_SLUG;
    case MONS_GIANT_SNAIL:
        return TILE_MONS_GIANT_SNAIL;
    case MONS_SPATIAL_VORTEX:
        return TILE_MONS_SPATIAL_VORTEX;
    case MONS_PIT_FIEND:
        return TILE_MONS_PIT_FIEND;
    case MONS_BORING_BEETLE:
        return TILE_MONS_BORING_BEETLE;
    case MONS_GARGOYLE:
        return TILE_MONS_GARGOYLE;
    case MONS_METAL_GARGOYLE:
        return TILE_MONS_METAL_GARGOYLE;
    case MONS_MOLTEN_GARGOYLE:
        return TILE_MONS_MOLTEN_GARGOYLE;
    case MONS_PROGRAM_BUG:
        return TILE_MONS_PROGRAM_BUG;
    case MONS_MNOLEG:
        return TILE_MONS_MNOLEG;
    case MONS_LOM_LOBON:
        return TILE_MONS_LOM_LOBON;
    case MONS_CEREBOV:
        return TILE_MONS_CEREBOV;
    case MONS_GLOORX_VLOQ:
        return TILE_MONS_GLOORX_VLOQ;
    case MONS_MOLLUSC_LORD:
        return TILE_TODO;
    case MONS_NAGA_MAGE:
        return TILE_MONS_NAGA_MAGE;
    case MONS_NAGA_WARRIOR:
        return TILE_MONS_NAGA_WARRIOR;
    case MONS_ORC_WARLORD:
        return TILE_MONS_ORC_WARLORD;
    case MONS_DEEP_ELF_SOLDIER:
        return TILE_MONS_DEEP_ELF_SOLDIER;
    case MONS_DEEP_ELF_FIGHTER:
        return TILE_MONS_DEEP_ELF_FIGHTER;
    case MONS_DEEP_ELF_KNIGHT:
        return TILE_MONS_DEEP_ELF_KNIGHT;
    case MONS_DEEP_ELF_MAGE:
        return TILE_MONS_DEEP_ELF_MAGE;
    case MONS_DEEP_ELF_SUMMONER:
        return TILE_MONS_DEEP_ELF_SUMMONER;
    case MONS_DEEP_ELF_CONJURER:
        return TILE_MONS_DEEP_ELF_CONJURER;
    case MONS_DEEP_ELF_PRIEST:
        return TILE_MONS_DEEP_ELF_PRIEST;
    case MONS_DEEP_ELF_HIGH_PRIEST:
        return TILE_MONS_DEEP_ELF_HIGH_PRIEST;
    case MONS_DEEP_ELF_DEMONOLOGIST:
        return TILE_MONS_DEEP_ELF_DEMONOLOGIST;
    case MONS_DEEP_ELF_ANNIHILATOR:
        return TILE_MONS_DEEP_ELF_ANNIHILATOR;
    case MONS_DEEP_ELF_SORCERER:
        return TILE_MONS_DEEP_ELF_SORCERER;
    case MONS_DEEP_ELF_DEATH_MAGE:
        return TILE_MONS_DEEP_ELF_DEATH_MAGE;
    case MONS_BROWN_OOZE:
        return TILE_MONS_BROWN_OOZE;
    case MONS_AZURE_JELLY:
        return TILE_MONS_AZURE_JELLY;
    case MONS_DEATH_OOZE:
        return TILE_MONS_DEATH_OOZE;
    case MONS_ACID_BLOB:
        return TILE_MONS_ACID_BLOB;
    case MONS_ROYAL_JELLY:
        return TILE_MONS_ROYAL_JELLY;
    case MONS_TERENCE:
        return TILE_MONS_TERENCE;
    case MONS_JESSICA:
        return TILE_MONS_JESSICA;
    case MONS_IJYB:
        return TILE_MONS_IJYB;
    case MONS_SIGMUND:
        return TILE_MONS_SIGMUND;
    case MONS_BLORK_THE_ORC:
        return TILE_MONS_BLORK_THE_ORC;
    case MONS_EDMUND:
        return TILE_MONS_EDMUND;
    case MONS_PSYCHE:
        return TILE_MONS_PSYCHE;
    case MONS_EROLCHA:
        return TILE_MONS_EROLCHA;
    case MONS_DONALD:
        return TILE_MONS_DONALD;
    case MONS_URUG:
        return TILE_MONS_URUG;
    case MONS_MICHAEL:
        return TILE_MONS_MICHAEL;
    case MONS_JOSEPH:
        return TILE_MONS_JOSEPH;
    case MONS_SNORG:
        return TILE_MONS_SNORG;
    case MONS_ERICA:
        return TILE_MONS_ERICA;
    case MONS_JOSEPHINE:
        return TILE_MONS_JOSEPHINE;
    case MONS_HAROLD:
        return TILE_MONS_HAROLD;
    case MONS_NORBERT:
        return TILE_MONS_NORBERT;
    case MONS_JOZEF:
        return TILE_MONS_JOZEF;
    case MONS_AGNES:
        return TILE_MONS_AGNES;
    case MONS_MAUD:
        return TILE_MONS_MAUD;
    case MONS_LOUISE:
        return TILE_MONS_LOUISE;
    case MONS_FRANCIS:
        return TILE_MONS_FRANCIS;
    case MONS_FRANCES:
        return TILE_MONS_FRANCES;
    case MONS_RUPERT:
        return TILE_MONS_RUPERT;
    case MONS_WAYNE:
        return TILE_MONS_WAYNE;
    case MONS_DUANE:
        return TILE_MONS_DUANE;
    case MONS_XTAHUA:
        return TILE_MONS_XTAHUA;
    case MONS_NORRIS:
        return TILE_MONS_NORRIS;
    case MONS_FREDERICK:
        return TILE_MONS_FREDERICK;
    case MONS_MARGERY:
        return TILE_MONS_MARGERY;
    case MONS_POLYPHEMUS:
        return TILE_MONS_POLYPHEMUS;
    case MONS_BORIS:
        return TILE_MONS_BORIS;
    // Draconians handled above
    case MONS_MURRAY:
        return TILE_MONS_MURRAY;
    case MONS_TIAMAT:
        return TILE_MONS_TIAMAT;
    case MONS_DEEP_ELF_BLADEMASTER:
        return TILE_MONS_DEEP_ELF_BLADEMASTER;
    case MONS_DEEP_ELF_MASTER_ARCHER:
        return TILE_MONS_DEEP_ELF_MASTER_ARCHER;

    case MONS_GERYON:
        return TILE_MONS_GERYON;
    case MONS_DISPATER:
        return TILE_MONS_DISPATER;
    case MONS_ASMODEUS:
        return TILE_MONS_ASMODEUS;
    case MONS_ANTAEUS:
        return TILE_MONS_ANTAEUS;
    case MONS_ERESHKIGAL:
        return TILE_MONS_ERESHKIGAL;

    case MONS_ANCIENT_LICH:
        return TILE_MONS_ANCIENT_LICH;
    case MONS_OOZE:
        return TILE_MONS_OOZE;
    case MONS_VAULT_GUARD:
        return TILE_MONS_VAULT_GUARD;
    case MONS_CURSE_SKULL:
        return TILE_MONS_CURSE_SKULL;
    case MONS_VAMPIRE_KNIGHT:
        return TILE_MONS_VAMPIRE_KNIGHT;
    case MONS_VAMPIRE_MAGE:
        return TILE_MONS_VAMPIRE_MAGE;
    case MONS_SHINING_EYE:
        return TILE_MONS_SHINING_EYE;
    case MONS_ORB_GUARDIAN:
        return TILE_MONS_ORB_GUARDIAN;
    case MONS_DAEVA:
        return TILE_MONS_DAEVA;
    case MONS_SPECTRAL_THING:
        return TILE_MONS_SPECTRAL_THING;
    case MONS_GREATER_NAGA:
        return TILE_MONS_GREATER_NAGA;
    case MONS_SKELETAL_DRAGON:
        return TILE_MONS_SKELETAL_DRAGON;
    case MONS_TENTACLED_MONSTROSITY:
        return TILE_MONS_TENTACLED_MONSTROSITY;
    case MONS_SPHINX:
        return TILE_MONS_SPHINX;
    case MONS_ROTTING_HULK:
        return TILE_MONS_ROTTING_HULK;
    case MONS_GUARDIAN_MUMMY:
        return TILE_MONS_GUARDIAN_MUMMY;
    case MONS_GREATER_MUMMY:
        return TILE_MONS_GREATER_MUMMY;
    case MONS_MUMMY_PRIEST:
        return TILE_MONS_MUMMY_PRIEST;
    case MONS_CENTAUR_WARRIOR:
        return TILE_MONS_CENTAUR_WARRIOR;
    case MONS_YAKTAUR_CAPTAIN:
        return TILE_MONS_YAKTAUR_CAPTAIN;
    case MONS_KILLER_KLOWN:
        return TILE_MONS_KILLER_KLOWN;
    case MONS_ELECTRIC_GOLEM:
        return TILE_MONS_ELECTRIC_GOLEM;
    case MONS_BALL_LIGHTNING:
        return TILE_MONS_BALL_LIGHTNING;
    case MONS_ORB_OF_FIRE:
        return TILE_MONS_ORB_OF_FIRE;
    case MONS_QUOKKA:
        return TILE_MONS_QUOKKA;
    case MONS_EYE_OF_DEVASTATION:
        return TILE_MONS_EYE_OF_DEVASTATION;
    case MONS_MOTH_OF_WRATH:
        return TILE_MONS_MOTH_OF_WRATH;
    case MONS_DEATH_COB:
        return TILE_MONS_DEATH_COB;
    case MONS_CURSE_TOE:
        return TILE_MONS_CURSE_TOE;

    case MONS_GOLD_MIMIC:
    case MONS_WEAPON_MIMIC:
    case MONS_ARMOUR_MIMIC:
    case MONS_SCROLL_MIMIC:
    case MONS_POTION_MIMIC:
    {
      // Use item tile
      item_def  item;
      get_mimic_item( mon, item );
      return tileidx_item(item);
    }

    case MONS_HELL_HOG:
        return TILE_MONS_HELL_HOG;
    case MONS_SERPENT_OF_HELL:
        return TILE_MONS_SERPENT_OF_HELL;
    case MONS_BOGGART:
        return TILE_MONS_BOGGART;
    case MONS_QUICKSILVER_DRAGON:
        return TILE_MONS_QUICKSILVER_DRAGON;
    case MONS_IRON_DRAGON:
        return TILE_MONS_IRON_DRAGON;
    case MONS_SKELETAL_WARRIOR:
        return TILE_MONS_SKELETAL_WARRIOR;
    case MONS_PLAYER_GHOST:
        return TILE_MONS_PLAYER_GHOST;
    case MONS_PANDEMONIUM_DEMON:
        return TILE_MONS_PANDEMONIUM_DEMON;
    case MONS_GIANT_NEWT:
        return TILE_MONS_GIANT_NEWT;
    case MONS_GIANT_GECKO:
        return TILE_MONS_GIANT_GECKO;
    case MONS_GIANT_IGUANA:
        return TILE_MONS_GIANT_IGUANA;
    case MONS_GILA_MONSTER:
        return TILE_MONS_GILA_MONSTER;
    case MONS_KOMODO_DRAGON:
        return TILE_MONS_KOMODO_DRAGON;
    case MONS_LAVA_WORM:
        return TILE_MONS_LAVA_WORM;
    case MONS_LAVA_FISH:
        return TILE_MONS_LAVA_FISH;
    case MONS_LAVA_SNAKE:
        return TILE_MONS_LAVA_SNAKE;
    case MONS_SALAMANDER:
        return TILE_MONS_SALAMANDER;
    case MONS_BIG_FISH:
        return TILE_MONS_BIG_FISH;
    case MONS_GIANT_GOLDFISH:
        return TILE_MONS_GIANT_GOLDFISH;
    case MONS_ELECTRICAL_EEL:
        return TILE_MONS_ELECTRICAL_EEL;
    case MONS_JELLYFISH:
        return TILE_MONS_JELLYFISH;
    case MONS_WATER_ELEMENTAL:
        return TILE_MONS_WATER_ELEMENTAL;
    case MONS_SWAMP_WORM:
        return TILE_MONS_SWAMP_WORM;
    case MONS_ROCK_WORM:
        return TILE_MONS_ROCK_WORM;
    case MONS_ORANGE_STATUE:
        return TILE_DNGN_ORANGE_CRYSTAL_STATUE;
    case MONS_SILVER_STATUE:
        return TILE_DNGN_SILVER_STATUE;
    case MONS_ICE_STATUE:
        return TILE_DNGN_ICE_STATUE;
    case MONS_DEATH_DRAKE:
        return TILE_MONS_DEATH_DRAKE;
    case MONS_MERFOLK:
        if (in_water)
            return TILE_MONS_MERFOLK_FIGHTER_WATER;
        else
            return TILE_MONS_MERFOLK_FIGHTER;
    case MONS_MERMAID:
        if (in_water)
            return TILE_MONS_MERMAID_WATER;
        else
            return TILE_MONS_MERMAID;
    }

    return TILE_ERROR;
}

static int _tileidx_monster(int mon_idx, bool detected)
{
    ASSERT(mon_idx != -1);
    int ch = _tileidx_monster_base(mon_idx, detected);
    const monsters* mons = &menv[mon_idx];

    if (mons_flies(mons))
        ch |= TILE_FLAG_FLYING;
    if (mons->has_ench(ENCH_HELD))
        ch |= TILE_FLAG_NET;
    if (mons->has_ench(ENCH_POISON))
        ch |= TILE_FLAG_POISON;

    if (mons_friendly(mons))
    {
        ch |= TILE_FLAG_PET;
    }
    else if (mons_neutral(mons))
    {
        ch |= TILE_FLAG_NEUTRAL;
    }
    else if (mons_looks_stabbable(mons))
    {
        ch |= TILE_FLAG_STAB;
    }
    else if (mons_looks_distracted(mons))
    {
        ch |= TILE_FLAG_MAY_STAB;
    }
    return ch;
}

static int _tileidx_fixed_artefact(int special)
{
  switch(special)
  {
  case SPWPN_SINGING_SWORD:       return TILE_SPWPN_SINGING_SWORD;
  case SPWPN_WRATH_OF_TROG:       return TILE_SPWPN_WRATH_OF_TROG;
  case SPWPN_SCYTHE_OF_CURSES:    return TILE_SPWPN_SCYTHE_OF_CURSES;
  case SPWPN_MACE_OF_VARIABILITY: return TILE_SPWPN_MACE_OF_VARIABILITY;
  case SPWPN_GLAIVE_OF_PRUNE:     return TILE_SPWPN_GLAIVE_OF_PRUNE;
  case SPWPN_SCEPTRE_OF_TORMENT:  return TILE_SPWPN_SCEPTRE_OF_TORMENT;
  case SPWPN_SWORD_OF_ZONGULDROK: return TILE_SPWPN_SWORD_OF_ZONGULDROK;
  case SPWPN_SWORD_OF_CEREBOV:    return TILE_SPWPN_SWORD_OF_CEREBOV;
  case SPWPN_STAFF_OF_DISPATER:   return TILE_SPWPN_STAFF_OF_DISPATER;
  case SPWPN_SCEPTRE_OF_ASMODEUS: return TILE_SPWPN_SCEPTRE_OF_ASMODEUS;
  case SPWPN_SWORD_OF_POWER:      return TILE_SPWPN_SWORD_OF_POWER;
  case SPWPN_KNIFE_OF_ACCURACY:   return TILE_SPWPN_KNIFE_OF_ACCURACY;
  case SPWPN_STAFF_OF_OLGREB:     return TILE_SPWPN_STAFF_OF_OLGREB;
  case SPWPN_VAMPIRES_TOOTH:      return TILE_SPWPN_VAMPIRES_TOOTH;
  case SPWPN_STAFF_OF_WUCAD_MU:   return TILE_SPWPN_STAFF_OF_WUCAD_MU;
  }
  
  return TILE_ERROR;
}

static int _tileidx_unrand_artefact(int idx)
{
    switch (idx)
    {
        case 1: return TILE_URAND_BLOODBANE;
        case 2: return TILE_URAND_SHADOWS;
        case 3: return TILE_URAND_FLAMING_DEATH;
        case 4: return TILE_URAND_IGNORANCE;
        case 5: return TILE_URAND_ZIN;
        case 6: return TILE_URAND_AUGMENTATION;
        case 7: return TILE_URAND_BRILLIANCE;
        case 8: return TILE_URAND_THIEF;
        case 9: return TILE_URAND_BULLSEYE;
        case 10: return TILE_URAND_DYROVEPREVA;
        case 11: return TILE_URAND_LEECH;
        case 12: return TILE_URAND_CEKUGOB;
        case 13: return TILE_URAND_MISFORTUNE;
        case 14: return TILE_URAND_CHILLY_DEATH;
        case 15: return TILE_URAND_FOUR_WINDS;
        case 16: return TILE_URAND_MORG;
        case 17: return TILE_URAND_FINISHER;
        case 18: return TILE_URAND_PUNK;
        case 19: return TILE_URAND_KRISHNA;
        case 20: return TILE_URAND_FLASH;
        case 21: return TILE_URAND_SKULLCRUSHER;
        case 22: return TILE_URAND_ASSASSIN;
        case 23: return TILE_URAND_GUARD;
        case 24: return TILE_URAND_JIHAD;
        case 25: return TILE_URAND_LEAR;
        case 26: return TILE_URAND_ZHOR;
        case 27: return TILE_URAND_FIERY_DEVIL;
        case 28: return TILE_URAND_SALAMANDER;
        case 29: return TILE_URAND_WAR;
        case 30: return TILE_URAND_DOOM_KNIGHT;
        case 31: return TILE_URAND_RESISTANCE;
        case 32: return TILE_URAND_FOLLY;
        case 33: return TILE_URAND_BLOODLUST;
        case 34: return TILE_URAND_EOS;
        case 35: return TILE_URAND_SHAOLIN;
        case 36: return TILE_URAND_ROBUSTNESS;
        case 37: return TILE_URAND_EDISON;
        case 38: return TILE_URAND_VOO_DOO;
        case 39: return TILE_URAND_OCTOPUS_KING;
        case 40: return TILE_URAND_DRAGONMASK;
        case 41: return TILE_URAND_ARGA;
        case 42: return TILE_URAND_ELEMENTAL;
        case 43: return TILE_URAND_SNIPER;
        case 44: return TILE_URAND_ERCHIDEL;
        case 45: return TILE_URAND_NIGHT;
        case 46: return TILE_URAND_PLUTONIUM;
        case 47: return TILE_URAND_UNDERADHUNTER;
        case 48: return TILE_URAND_DRAGON_KING;
        case 49: return TILE_URAND_ALCHEMIST;
        case 50: return TILE_URAND_FENCER;
        case 51: return TILE_URAND_MAGE;
        case 52: return TILE_URAND_BLOWGUN;
        case 53: //return TILE_URAND_WYRMBANE;
                 return TILE_WPN_SPEAR + 1;
        case 54: //return TILE_URAND_SPRIGGAN;
                 return TILE_WPN_KNIFE + 1;
        case 55: //return TILE_URAND_LAJATANG;
                 return TILE_WPN_LAJATANG + 1;
        case 56: //return TILE_URAND_BROOCH;
                 return TILE_AMU_RANDOM_OFFSET + 2;
        case 57: //return TILE_URAND_SERPENT_SCOURGE;
                 return TILE_WPN_WHIP + 1;
        default: return TILE_TODO;
    }
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

static int _tileidx_weapon(const item_def &item)
{
    static const int etable[4][4] = {
      {0, 0, 0, 0}, // No ego tile
      {0, 1, 1, 1}, // One ego tile
      {0, 1, 1, 2}, // Two ego tile
      {0, 1, 2, 3}
    };

    int race  = item.flags & ISFLAG_RACIAL_MASK;
    int etype = _get_etype(item);

    if (etype > 1)
        etype--;

    switch (item.sub_type)
    {
    case WPN_KNIFE:
        return TILE_WPN_KNIFE;

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
        return TILE_WPN_SHORT_SWORD + etable[1][etype];

    case WPN_QUICK_BLADE:
        return TILE_WPN_QUICK_BLADE;

    case WPN_SABRE:
        return TILE_WPN_SABRE + etable[1][etype];

    case WPN_FALCHION:
        return TILE_WPN_FALCHION;

    case WPN_KATANA:
        return TILE_WPN_KATANA + etable[1][etype];

    case WPN_LONG_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_LONG_SWORD_ORC;
        return TILE_WPN_LONG_SWORD + etable[1][etype];

    case WPN_GREAT_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_GREAT_SWORD_ORC;
        return TILE_WPN_GREAT_SWORD + etable[1][etype];

    case WPN_SCIMITAR:
        return TILE_WPN_SCIMITAR + etable[1][etype];

    case WPN_DOUBLE_SWORD:
        return TILE_WPN_DOUBLE_SWORD;

    case WPN_TRIPLE_SWORD:
        return TILE_WPN_TRIPLE_SWORD;

    case WPN_HAND_AXE:
        return TILE_WPN_HAND_AXE;

    case WPN_WAR_AXE:
        return TILE_WPN_WAR_AXE;

    case WPN_BROAD_AXE:
        return TILE_WPN_BROAD_AXE + etable[1][etype];

    case WPN_BATTLEAXE:
        return TILE_WPN_BATTLEAXE + etable[1][etype];

    case WPN_EXECUTIONERS_AXE:
        return TILE_WPN_EXECUTIONERS_AXE + etable[1][etype];

    case WPN_BLOWGUN:
        return TILE_WPN_BLOWGUN + etable[1][etype];

    case WPN_SLING:
        return TILE_WPN_SLING;

    case WPN_BOW:
        return TILE_WPN_BOW + etable[1][etype];

    case WPN_CROSSBOW:
        return TILE_WPN_CROSSBOW + etable[1][etype];

    case WPN_HAND_CROSSBOW:
        return TILE_WPN_HAND_CROSSBOW + etable[1][etype];

    case WPN_SPEAR:
        return TILE_WPN_SPEAR + etable[1][etype];

    case WPN_TRIDENT:
        return TILE_WPN_TRIDENT + etable[1][etype];

    case WPN_HALBERD:
        return TILE_WPN_HALBERD + etable[1][etype];

    case WPN_SCYTHE:
        return TILE_WPN_SCYTHE + etable[1][etype];

    case WPN_GLAIVE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_GLAIVE_ORC;
        return TILE_WPN_GLAIVE + etable[1][etype];

    case WPN_QUARTERSTAFF:
        return TILE_WPN_QUARTERSTAFF;

    case WPN_CLUB:
        return TILE_WPN_CLUB;

    case WPN_HAMMER:
        return TILE_WPN_HAMMER + etable[1][etype];

    case WPN_MACE:
        return TILE_WPN_MACE + etable[1][etype];

    case WPN_FLAIL:
        return TILE_WPN_FLAIL + etable[1][etype];

    case WPN_SPIKED_FLAIL:
        return TILE_WPN_SPIKED_FLAIL + etable[1][etype];

    case WPN_GREAT_MACE:
        return TILE_WPN_GREAT_MACE + etable[1][etype];

    case WPN_DIRE_FLAIL:
        return TILE_WPN_GREAT_FLAIL + etable[1][etype];

    case WPN_MORNINGSTAR:
        return TILE_WPN_MORNINGSTAR + etable[1][etype];

    case WPN_EVENINGSTAR:
        return TILE_WPN_EVENINGSTAR + etable[1][etype];

    case WPN_GIANT_CLUB:
        return TILE_WPN_GIANT_CLUB;

    case WPN_GIANT_SPIKED_CLUB:
        return TILE_WPN_GIANT_SPIKED_CLUB;

    case WPN_ANCUS:
        return TILE_WPN_ANCUS;

    case WPN_WHIP:
        return TILE_WPN_WHIP;

    case WPN_DEMON_BLADE:
        return TILE_WPN_DEMON_BLADE;

    case WPN_DEMON_WHIP:
        return TILE_WPN_DEMON_WHIP;

    case WPN_DEMON_TRIDENT:
        return TILE_WPN_DEMON_TRIDENT;

    case WPN_BLESSED_BLADE:
        return TILE_WPN_BLESSED_BLADE;

    case WPN_LONGBOW:
        return TILE_WPN_LONGBOW;

    case WPN_LAJATANG:
        return TILE_WPN_LAJATANG;

    case WPN_BARDICHE:
        return TILE_WPN_LOCHABER_AXE;
    }
  
    return TILE_ERROR;
}

static int _tileidx_missile(const item_def &item)
{
    int brand = item.special;
    switch(item.sub_type)
    {
    case MI_STONE:        return TILE_MI_STONE;
    case MI_ARROW:        return TILE_MI_ARROW;
    case MI_BOLT:         return TILE_MI_BOLT;
    case MI_LARGE_ROCK:   return TILE_MI_LARGE_ROCK;
    case MI_SLING_BULLET: return TILE_MI_SLING_BULLET;
    case MI_JAVELIN:      return TILE_MI_JAVELIN;
    case MI_THROWING_NET: return TILE_MI_THROWING_NET;

    case MI_DART:
        if (brand == SPMSL_POISONED || brand == SPMSL_POISONED_II)
            return TILE_MI_DART_P;
        return TILE_MI_DART;

    case MI_NEEDLE: 
        if (brand == SPMSL_POISONED || brand == SPMSL_POISONED_II)
            return TILE_MI_NEEDLE_P;
        return TILE_MI_NEEDLE;
  }
  
  return TILE_ERROR;
}

static int _tileidx_armour(const item_def &item)
{
    int race  = item.flags & ISFLAG_RACIAL_MASK;
    int type  = item.sub_type;
    int etype = _get_etype(item);

    static const int etable[5][5] = {
        {0, 0, 0, 0, 0}, // No ego tile
        {0, 1, 1, 1, 1}, // One ego tile
        {0, 1, 1, 1, 2}, // Two ego tile
        {0, 1, 1, 2, 3},
        {0, 1, 2, 3, 4}
    };

    switch(type)
    {
    case ARM_ROBE: 
        return TILE_ARM_ROBE + etable[2][etype];
        
    case ARM_LEATHER_ARMOUR:
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_LEATHER_ARMOUR_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_LEATHER_ARMOUR_ELF;
        return TILE_ARM_LEATHER_ARMOUR  + etable[2][etype];
        
    case ARM_RING_MAIL:
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_RING_MAIL_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_RING_MAIL_ELF;
        if (race == ISFLAG_DWARVEN)
            return TILE_ARM_RING_MAIL_DWA;
        return TILE_ARM_RING_MAIL + etable[1][etype];
        
    case ARM_SCALE_MAIL:
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_SCALE_MAIL_ELF;
        return TILE_ARM_SCALE_MAIL + etable[1][etype];

    case ARM_CHAIN_MAIL:
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_CHAIN_MAIL_ELF;
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_CHAIN_MAIL_ORC;
        return TILE_ARM_CHAIN_MAIL + etable[1][etype];

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
        return TILE_ARM_SHIELD  + etable[2][etype];

    case ARM_CLOAK: 
        return TILE_ARM_CLOAK + etable[3][etype];

    case ARM_WIZARD_HAT:
        return TILE_THELM_WIZARD_HAT + etable[1][etype];

    case ARM_CAP:
        return TILE_THELM_CAP;

    case ARM_HELMET:
        return TILE_THELM_HELM  + etable[3][etype];

    case ARM_GLOVES:
        return TILE_ARM_GLOVES  + etable[3][etype];

    case ARM_BOOTS: 
        return TILE_ARM_BOOTS + etable[3][etype];

    case ARM_BUCKLER: 
        return TILE_ARM_BUCKLER + etable[1][etype];

    case ARM_LARGE_SHIELD:
        return TILE_ARM_LARGE_SHIELD + etable[2][etype];

    case ARM_CENTAUR_BARDING:
        return TILE_ARM_CENTAUR_BARDING + etable[3][etype];

    case ARM_NAGA_BARDING:
        return TILE_ARM_NAGA_BARDING + etable[3][etype];

    case ARM_ANIMAL_SKIN:
        return TILE_ARM_ANIMAL_SKIN + etable[1][etype];

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

    case FOOD_CHUNK: 
        if (food_is_rotten(item))
            return TILE_FOOD_CHUNK_ROTTEN;
        return TILE_FOOD_CHUNK;
    }

    return TILE_ERROR;
}

// returns index of corpse tiles
// parameter mon already holds the corpse type (monster species)
static int _tileidx_corpse(int mon)
{
  switch (mon)
  {
  case MONS_GIANT_COCKROACH:      return TILE_CORPSE_GIANT_COCKROACH;
  case MONS_GIANT_ANT:            return TILE_CORPSE_GIANT_ANT;
  case MONS_SOLDIER_ANT:          return TILE_CORPSE_SOLDIER_ANT;

  case MONS_GIANT_BAT:            return TILE_CORPSE_GIANT_BAT;
  case MONS_BUTTERFLY:            return TILE_CORPSE_BUTTERFLY;

  // including centaur warrior
  case MONS_CENTAUR:              return TILE_CORPSE_CENTAUR;
  // including yaktaur captain
  case MONS_YAKTAUR:              return TILE_CORPSE_YAKTAUR;

  // draconians
  case MONS_DRACONIAN:            return TILE_CORPSE_DRACONIAN_BROWN;
  case MONS_BLACK_DRACONIAN:      return TILE_CORPSE_DRACONIAN_BLACK;
  case MONS_YELLOW_DRACONIAN:     return TILE_CORPSE_DRACONIAN_YELLOW;
  case MONS_GREEN_DRACONIAN:      return TILE_CORPSE_DRACONIAN_GREEN;
  case MONS_MOTTLED_DRACONIAN:    return TILE_CORPSE_DRACONIAN_MOTTLED;
  case MONS_PALE_DRACONIAN:       return TILE_CORPSE_DRACONIAN_PALE;
  case MONS_PURPLE_DRACONIAN:     return TILE_CORPSE_DRACONIAN_PURPLE;
  case MONS_RED_DRACONIAN:        return TILE_CORPSE_DRACONIAN_RED;
  case MONS_WHITE_DRACONIAN:      return TILE_CORPSE_DRACONIAN_WHITE;

  // all elves
  case MONS_ELF:                  return TILE_CORPSE_ELF;
  
  case MONS_JACKAL:               return TILE_CORPSE_JACKAL;
  case MONS_HOUND:                return TILE_CORPSE_HOUND;
  case MONS_WAR_DOG:              return TILE_CORPSE_WAR_DOG;
  case MONS_HOG:                  return TILE_CORPSE_HOG;
  case MONS_WOLF:                 return TILE_CORPSE_WOLF;
  case MONS_WARG:                 return TILE_CORPSE_WARG;

  case MONS_GOBLIN:               return TILE_CORPSE_GOBLIN;
  case MONS_HOBGOBLIN:            return TILE_CORPSE_HOBGOBLIN;
  case MONS_GNOLL:                return TILE_CORPSE_GNOLL;

  case MONS_KILLER_BEE:           return TILE_CORPSE_KILLER_BEE;
  case MONS_KILLER_BEE_LARVA:     return TILE_CORPSE_KILLER_BEE_LARVA;
  case MONS_BUMBLEBEE:            return TILE_CORPSE_BUMBLEBEE;

  case MONS_ELEPHANT_SLUG:        return TILE_CORPSE_ELEPHANT_SLUG;
  case MONS_GIANT_SLUG:           return TILE_CORPSE_GIANT_SLUG;
  case MONS_GIANT_SNAIL:          return TILE_CORPSE_GIANT_SNAIL;

  case MONS_GIANT_NEWT:           return TILE_CORPSE_GIANT_NEWT;
  case MONS_GIANT_GECKO:          return TILE_CORPSE_GIANT_GECKO;
  case MONS_GIANT_IGUANA:         return TILE_CORPSE_GIANT_IGUANA;
  case MONS_GIANT_LIZARD:         return TILE_CORPSE_GIANT_LIZARD;
  case MONS_GILA_MONSTER:         return TILE_CORPSE_GILA_MONSTER;
  case MONS_LINDWURM:             return TILE_CORPSE_LINDWURM;
  case MONS_KOMODO_DRAGON:        return TILE_CORPSE_KOMODO_DRAGON;
  case MONS_SWAMP_DRAKE:          return TILE_CORPSE_SWAMP_DRAKE;
  case MONS_FIREDRAKE:            return TILE_CORPSE_FIREDRAKE;
  case MONS_DEATH_DRAKE:          return TILE_CORPSE_DEATH_DRAKE;

  case MONS_MERMAID:              return TILE_CORPSE_MERMAID;
  case MONS_MERFOLK:              return TILE_CORPSE_MERFOLK_FIGHTER;
  
  case MONS_NECROPHAGE:           return TILE_CORPSE_NECROPHAGE;
  case MONS_GHOUL:                return TILE_CORPSE_GHOUL;

  // all orcs
  case MONS_ORC:                  return TILE_CORPSE_ORC;
  
  case MONS_RAT:                  return TILE_CORPSE_RAT;
  case MONS_QUOKKA:               return TILE_CORPSE_QUOKKA;
  case MONS_GREY_RAT:             return TILE_CORPSE_GREY_RAT;
  case MONS_GREEN_RAT:            return TILE_CORPSE_GREEN_RAT;
  case MONS_ORANGE_RAT:           return TILE_CORPSE_ORANGE_RAT;

  case MONS_GIANT_CENTIPEDE:      return TILE_CORPSE_GIANT_CENTIPEDE;
  case MONS_GIANT_MITE:           return TILE_CORPSE_GIANT_MITE;
  case MONS_SCORPION:             return TILE_CORPSE_SCORPION;
  case MONS_WOLF_SPIDER:          return TILE_CORPSE_WOLF_SPIDER;
  case MONS_REDBACK:              return TILE_CORPSE_REDBACK;

  case MONS_MINOTAUR:             return TILE_CORPSE_MINOTAUR;
  
  case MONS_UGLY_THING:           return TILE_CORPSE_UGLY_THING;
  case MONS_VERY_UGLY_THING:      return TILE_CORPSE_VERY_UGLY_THING;

  case MONS_WORM:                 return TILE_CORPSE_WORM;
  case MONS_BRAIN_WORM:           return TILE_CORPSE_BRAIN_WORM;
  case MONS_ANT_LARVA:            return TILE_CORPSE_ANT_LARVA;
  case MONS_SPINY_WORM:           return TILE_CORPSE_SPINY_WORM;

  case MONS_YELLOW_WASP:          return TILE_CORPSE_YELLOW_WASP;
  case MONS_GIANT_MOSQUITO:       return TILE_CORPSE_GIANT_MOSQUITO;
  case MONS_RED_WASP:             return TILE_CORPSE_RED_WASP;
  case MONS_GIANT_BLOWFLY:        return TILE_CORPSE_GIANT_BLOWFLY;

  case MONS_GIANT_BEETLE:         return TILE_CORPSE_GIANT_BEETLE;
  case MONS_BOULDER_BEETLE:       return TILE_CORPSE_BOULDER_BEETLE;
  case MONS_BORING_BEETLE:        return TILE_CORPSE_BORING_BEETLE;

  case MONS_CYCLOPS:              return TILE_CORPSE_CYCLOPS;
  case MONS_STONE_GIANT:          return TILE_CORPSE_STONE_GIANT;
  case MONS_TITAN:                return TILE_CORPSE_TITAN;
  case MONS_HILL_GIANT:           return TILE_CORPSE_HILL_GIANT;
  case MONS_FIRE_GIANT:           return TILE_CORPSE_FIRE_GIANT;
  case MONS_FROST_GIANT:          return TILE_CORPSE_FROST_GIANT;

  case MONS_WYVERN:               return TILE_CORPSE_WYVERN;
  case MONS_DRAGON:               return TILE_CORPSE_DRAGON;
  case MONS_HYDRA:                return TILE_CORPSE_HYDRA;
  case MONS_STEAM_DRAGON:         return TILE_CORPSE_STEAM_DRAGON;
  case MONS_ICE_DRAGON:           return TILE_CORPSE_ICE_DRAGON;
  case MONS_MOTTLED_DRAGON:       return TILE_CORPSE_MOTTLED_DRAGON;
  case MONS_STORM_DRAGON:         return TILE_CORPSE_STORM_DRAGON;
  case MONS_GOLDEN_DRAGON:        return TILE_CORPSE_GOLDEN_DRAGON;
  case MONS_SWAMP_DRAGON:         return TILE_CORPSE_SWAMP_DRAGON;
  case MONS_SHADOW_DRAGON:        return TILE_CORPSE_SHADOW_DRAGON;

  case MONS_GIANT_FROG:           return TILE_CORPSE_GIANT_FROG;
  case MONS_GIANT_BROWN_FROG:     return TILE_CORPSE_GIANT_BROWN_FROG;
  case MONS_SPINY_FROG:           return TILE_CORPSE_SPINY_FROG;
  case MONS_BLINK_FROG:           return TILE_CORPSE_BLINK_FROG;

  case MONS_GIANT_EYEBALL:        return TILE_CORPSE_GIANT_EYEBALL;
  case MONS_EYE_OF_DRAINING:      return TILE_CORPSE_EYE_OF_DRAINING;
  case MONS_GIANT_ORANGE_BRAIN:   return TILE_CORPSE_GIANT_ORANGE_BRAIN;
  case MONS_GREAT_ORB_OF_EYES:    return TILE_CORPSE_GREAT_ORB_OF_EYES;

  case MONS_MANTICORE:            return TILE_CORPSE_MANTICORE;
  case MONS_HIPPOGRIFF:           return TILE_CORPSE_HIPPOGRIFF;
  case MONS_GRIFFON:              return TILE_CORPSE_GRIFFON;

  // all kobolds
  case MONS_KOBOLD:               return TILE_CORPSE_KOBOLD;

  case MONS_GIANT_AMOEBA:         return TILE_CORPSE_GIANT_AMOEBA;
  // all nagas
  case MONS_NAGA:                 return TILE_CORPSE_NAGA;
  
  // including ogre-mage
  case MONS_OGRE:                 return TILE_CORPSE_OGRE;
  case MONS_TWO_HEADED_OGRE:      return TILE_CORPSE_TWO_HEADED_OGRE;
  
  case MONS_QUEEN_BEE:            return TILE_CORPSE_QUEEN_BEE;
  case MONS_QUEEN_ANT:            return TILE_CORPSE_QUEEN_ANT;

  case MONS_SMALL_SNAKE:          return TILE_CORPSE_SMALL_SNAKE;

  case MONS_SNAKE:                return TILE_CORPSE_SNAKE;
  case MONS_BLACK_SNAKE:          return TILE_CORPSE_BLACK_SNAKE;
  case MONS_BROWN_SNAKE:          return TILE_CORPSE_BROWN_SNAKE;
  case MONS_YELLOW_SNAKE:         return TILE_CORPSE_YELLOW_SNAKE;
  case MONS_GREY_SNAKE:           return TILE_CORPSE_GREY_SNAKE;

  case MONS_TROLL:                return TILE_CORPSE_TROLL;
  case MONS_ROCK_TROLL:           return TILE_CORPSE_ROCK_TROLL;
  case MONS_IRON_TROLL:           return TILE_CORPSE_IRON_TROLL;
  case MONS_DEEP_TROLL:           return TILE_CORPSE_DEEP_TROLL;

  case MONS_BEAR:                 return TILE_CORPSE_BEAR;
  case MONS_GRIZZLY_BEAR:         return TILE_CORPSE_GRIZZLY_BEAR;
  case MONS_POLAR_BEAR:           return TILE_CORPSE_POLAR_BEAR;
  case MONS_BLACK_BEAR:           return TILE_CORPSE_BLACK_BEAR;

  case MONS_SHEEP:                return TILE_CORPSE_SHEEP;
  case MONS_YAK:                  return TILE_CORPSE_YAK;
  case MONS_DEATH_YAK:            return TILE_CORPSE_DEATH_YAK;

  case MONS_GLOWING_SHAPESHIFTER: return TILE_CORPSE_GLOWING_SHAPESHIFTER; 
  case MONS_SHAPESHIFTER:         return TILE_CORPSE_SHAPESHIFTER;

  // all humans
  case MONS_HUMAN:                return TILE_CORPSE_HUMAN;
  }
  
  return TILE_ERROR;
}

static int _tileidx_misc(const item_def &item)
{
    switch(item.sub_type)
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
        return TILE_MISC_RUNE_OF_ZOT;
    }
    
    return TILE_ERROR;
}

/*****************************************************/
int tileidx_item(const item_def &item)
{
    int clas    = item.base_type;
    int type    = item.sub_type;
    int special = item.special;
    int color   = item.colour;

    id_arr& id = get_typeid_array();

    switch (clas)
    {
    case OBJ_WEAPONS:
        if (is_fixed_artefact(item))
            return _tileidx_fixed_artefact(special);
        else if (is_unrandom_artefact( item ))
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

        if (type < AMU_RAGE)
        {
            if(is_random_artefact( item ))
                return TILE_RING_RANDOM_OFFSET + color - 1;
            else 
                return TILE_RING_NORMAL_OFFSET + special % 13;
        }
        else
        {
            if (is_unrandom_artefact( item ))
                return _tileidx_unrand_artefact(find_unrandart_index(item));
            else if(is_random_artefact( item ))
                return TILE_AMU_RANDOM_OFFSET + color - 1;
            else
                return TILE_AMU_NORMAL_OFFSET + special % 13;
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
        type= special % 10;
        if (type < 2)
            return TILE_BOOK_PAPER_OFFSET + color;
        if (type == 2)
            return TILE_BOOK_LEATHER_OFFSET + special/10;
        if (type == 3)
            return TILE_BOOK_METAL_OFFSET + special/10;
        if (type == 4)
            return TILE_BOOK_PAPYRUS;

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
            return _tileidx_corpse(item.plus);

    case OBJ_ORBS:
        return TILE_ORB;

    case OBJ_MISCELLANY:
        return _tileidx_misc(item);

    default:
        return TILE_ERROR;
    }
}

/*
  Determine Octant of missile direction
  ---> X+
 |
 |  701
 Y  6O2
 +  543

  the octant boundary slope tan(pi/8)=sqrt(2)-1 = 0.414 is approximated by 2/5

*/
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
        switch(item.sub_type)
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
        switch(item.sub_type)
        {
            case MI_STONE:
                ch = TILE_MI_STONE0;
                break;
            case MI_SLING_BULLET:
                ch = TILE_MI_SLING_BULLET0;
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
            return ch;
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
        return TILE_ERROR;
    }
}

int tileidx_feature(int object, int gx, int gy)
{
    switch (object)
    {
    case DNGN_UNSEEN:
        return TILE_DNGN_UNSEEN;
    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
        return TILE_DNGN_ROCK_WALL_OFS;
    case DNGN_SECRET_DOOR:
        return (unsigned int)grid_secret_door_appearance(gx, gy);
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
    case DNGN_GRANITE_STATUE:
        return TILE_DNGN_GRANITE_STATUE;
    case DNGN_LAVA:
        return TILE_DNGN_LAVA;
    case DNGN_DEEP_WATER:
        return TILE_DNGN_DEEP_WATER;
    case DNGN_SHALLOW_WATER:
        return TILE_DNGN_SHALLOW_WATER;
    case DNGN_FLOOR: 
    case DNGN_UNDISCOVERED_TRAP:
       return TILE_DNGN_FLOOR;
    case DNGN_FLOOR_SPECIAL:
       return TILE_DNGN_FLOOR_SPECIAL;
    case DNGN_ENTER_HELL:
        return TILE_DNGN_ENTER_HELL;
    case DNGN_OPEN_DOOR:
        return TILE_DNGN_OPEN_DOOR;
    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_NATURAL:
        return _tileidx_trap(trap_type_at_xy(gx, gy));
    case DNGN_ENTER_SHOP:
        return TILE_DNGN_ENTER_SHOP;
    case DNGN_ENTER_LABYRINTH:
        return TILE_DNGN_ENTER_LABYRINTH;
    case DNGN_STONE_STAIRS_DOWN_I: 
    case DNGN_STONE_STAIRS_DOWN_II: 
    case DNGN_STONE_STAIRS_DOWN_III:
        return TILE_DNGN_STONE_STAIRS_DOWN;
    case DNGN_ESCAPE_HATCH_DOWN:
        return TILE_DNGN_ROCK_STAIRS_DOWN;
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        return TILE_DNGN_STONE_STAIRS_UP;
    case DNGN_ESCAPE_HATCH_UP:
        return TILE_DNGN_ROCK_STAIRS_UP;
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
       return TILE_DNGN_EXIT_ABYSS;
    case DNGN_STONE_ARCH:
        return TILE_DNGN_STONE_ARCH;
    case DNGN_ENTER_PANDEMONIUM:
        return TILE_DNGN_ENTER_PANDEMONIUM;
    case DNGN_EXIT_PANDEMONIUM:
        return TILE_DNGN_EXIT_PANDEMONIUM;
    case DNGN_TRANSIT_PANDEMONIUM:
        return TILE_DNGN_TRANSIT_PANDEMONIUM;
    case DNGN_ENTER_ORCISH_MINES:
    case DNGN_ENTER_HIVE:
    case DNGN_ENTER_LAIR:
    case DNGN_ENTER_SLIME_PITS:
    case DNGN_ENTER_VAULTS:
    case DNGN_ENTER_CRYPT:
    case DNGN_ENTER_HALL_OF_BLADES:
    case DNGN_ENTER_ZOT:
    case DNGN_ENTER_TEMPLE:
    case DNGN_ENTER_SNAKE_PIT:
    case DNGN_ENTER_ELVEN_HALLS:
    case DNGN_ENTER_TOMB:
    case DNGN_ENTER_SWAMP:
    case DNGN_ENTER_SHOALS:
    case DNGN_ENTER_RESERVED_2:
    case DNGN_ENTER_RESERVED_3:
    case DNGN_ENTER_RESERVED_4:
       return TILE_DNGN_ENTER;
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
    case DNGN_RETURN_FROM_ZOT:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_ELVEN_HALLS:
    case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_SHOALS:
    case DNGN_RETURN_RESERVED_2:
    case DNGN_RETURN_RESERVED_3:
    case DNGN_RETURN_RESERVED_4:
       return TILE_DNGN_RETURN;
    case DNGN_ENTER_PORTAL_VAULT:
    case DNGN_EXIT_PORTAL_VAULT:
       return TILE_DNGN_TRANSIT_PANDEMONIUM;
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
    }

    return TILE_ERROR;
}

static int _tileidx_cloud(int type, int decay)
{
    int ch = TILE_ERROR;
    int dur = decay/20;
    if (dur > 2)
        dur = 2;

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

        case CLOUD_PURP_SMOKE:
            ch = TILE_CLOUD_PURP_SMOKE;
            break;

        case CLOUD_MIASMA:
            ch = TILE_CLOUD_MIASMA;
            break;

        case CLOUD_BLACK_SMOKE:
            ch = TILE_CLOUD_BLACK_SMOKE;
            break;

        default:
            ch = TILE_CLOUD_GREY_SMOKE;
            break;
    }
    return (ch | TILE_FLAG_FLYING);
}

/**********************************************************/

int tileidx_player(int job)
{
    int ch = TILE_PLAYER;

    // Handle shapechange first
    switch (you.symbol)
    {
        case 's': ch = TILE_MONS_WOLF_SPIDER; break;
        case 'I': ch = TILE_MONS_ICE_BEAST;   break;
        case '8': ch = TILE_MONS_STONE_GOLEM; break;
        case 'D': ch = TILE_MONS_DRAGON;      break;
        case 'L': ch = TILE_MONS_LICH;        break;
        case '#': ch = TILE_MONS_VAPOUR;      break;
        case 'S': ch = TILE_MONS_LAVA_SNAKE;  break;
        case 'b': ch = TILE_MONS_GIANT_BAT;   break;
    }

    if (player_is_airborne())
        ch |= TILE_FLAG_FLYING;

    if (you.attribute[ATTR_HELD])
        ch |= TILE_FLAG_NET;

    return ch;
}


int tileidx_unseen(int ch, const coord_def& gc)
{
    int res = TILE_ERROR;
    ch &= 0xff;
    if (ch < 32)
        ch = 32;

    if (ch >= '@' && ch <= 'Z' || ch >= 'a' && ch <= 'z'
        || ch == '&' || ch >= '1' && ch <= '5' || ch == ';')
    {
        return TILE_UNSEEN_MONSTER | tile_unseen_flag(gc);
    }

    switch (ch)
    {
        //blank, walls, and floors first, since they are frequent
        case ' ': res = TILE_DNGN_UNSEEN; break;
        case 127: //old
        case 176:
        case 177: res = TILE_DNGN_ROCK_WALL_OFS; break;

        case 130:
        case ',':
        case '.':
        case 249:
        case 250: res = TILE_DNGN_FLOOR; break;

        case 137: res = TILE_DNGN_WAX_WALL; break;
        case 138: res = TILE_DNGN_STONE_WALL; break;
        case 139: res = TILE_DNGN_METAL_WALL; break;
        case 140: res = TILE_DNGN_GREEN_CRYSTAL_WALL; break;

        // others
        case '!': res = TILE_POTION_OFFSET + 13; break;
        case '"': res = TILE_AMU_NORMAL_OFFSET + 2; break;
        case '#': res = TILE_CLOUD_GREY_SMOKE; break;
        case '$': res = TILE_GOLD; break;
        case '%': res = TILE_FOOD_MEAT_RATION; break;
        case 142: res = TILE_UNSEEN_CORPSE; break;

        case '\'':
        case 134: res = TILE_DNGN_OPEN_DOOR; break;
        case '(':
        case ')': res = TILE_UNSEEN_WEAPON; break;
        case '*': res = TILE_DNGN_ROCK_WALL_OFS ; break;
        case '+': res =  TILE_BOOK_PAPER_OFFSET + 15; break;

        case '/': res = TILE_WAND_OFFSET; break;
        case '8': res = TILE_DNGN_SILVER_STATUE; break;
        case '<': res = TILE_DNGN_STONE_STAIRS_UP; break;
        case '=': res = TILE_RING_NORMAL_OFFSET + 1; break;
        case '>': res = TILE_DNGN_STONE_STAIRS_DOWN; break;
        case '?': res = TILE_SCROLL; break;
        case '[':
        case ']': res = TILE_UNSEEN_ARMOUR; break;
        case '\\': res = TILE_STAFF_OFFSET; break;
        case '^': res = TILE_DNGN_TRAP_ZOT; break;
        case '_':
        case 131: res = TILE_UNSEEN_ALTAR; break;
        case '~': res = TILE_UNSEEN_ITEM; break;
        case '{':
        case 135: res = TILE_DNGN_DEEP_WATER; break;
        case 133: res = TILE_DNGN_BLUE_FOUNTAIN; break;
        case '}': res = TILE_MISC_CRYSTAL_BALL_OF_SEEING; break;
        case 128: //old 
        case 254: res = TILE_DNGN_CLOSED_DOOR; break;
        case 129: res = TILE_DNGN_RETURN; break;
        case 132: res = TILE_UNSEEN_ENTRANCE; break;
        case 136: res = TILE_DNGN_ENTER; break;
        case 141: res = TILE_DNGN_LAVA; break;
    }
    //if(res == TILE_ERROR)printf("undefined mapchar %d [%c]\n",ch,ch);
    return res | tile_unseen_flag(gc);
}

int tileidx_bolt(const bolt &bolt)
{
    int col = bolt.colour;
    return tileidx_zap(col);
}

int tileidx_zap(int color)
{
    int col = color;
    if (col > 8) col -= 8;
    if (col < 1) col = 7;
    return TILE_SYM_BOLT_OFS -1 + col;
}

// Convert normal tile to 3D tile if it exists
// Plus modify wall tile index depending on
//  1: floor/wall flavor in 2D mode
//  2: connectivity in 3D mode
static void _finalize_tile(unsigned int *tile, bool is_special,
                           char wall_flv, char floor_flv, char special_flv)
{
    int orig = (*tile) & TILE_FLAG_MASK;
    int flag = (*tile) & (~TILE_FLAG_MASK);

    // Hack: Swap rock/stone in crypt and tomb, because there are
    //       only stone walls.
    if ((you.where_are_you == BRANCH_CRYPT || you.where_are_you == BRANCH_TOMB)
            && orig == TILE_DNGN_STONE_WALL)
            orig = TILE_DNGN_ROCK_WALL_OFS;

    // If there are special tiles for this level, then use them.
    // Otherwise, we'll fall through to the next case and replace
    // special tiles with normal floor.
    if (orig == TILE_DNGN_FLOOR && is_special &&
        get_num_floor_special_flavors() > 0)
    {
        (*tile) = get_floor_special_tile_idx() + special_flv;
        ASSERT(special_flv >= 0 && 
            special_flv < get_num_floor_special_flavors());
    }
    else if (orig == TILE_DNGN_FLOOR || orig == TILE_DNGN_FLOOR_SPECIAL)
    {
        (*tile) = get_floor_tile_idx() + floor_flv;
    }
    else if (orig == TILE_DNGN_ROCK_WALL_OFS)
    {
        (*tile) = get_wall_tile_idx() + wall_flv;
    }
    else if (orig == TILE_DNGN_SHALLOW_WATER ||
        orig == TILE_DNGN_DEEP_WATER ||
        orig == TILE_DNGN_LAVA ||
        orig == TILE_DNGN_STONE_WALL)
    {
        // These types always have four flavors...
        (*tile) = orig + (floor_flv % 4);
    }
    else if (orig == TILE_DNGN_CLOSED_DOOR || orig == TILE_DNGN_OPEN_DOOR)
    {
        ASSERT(special_flv >= 0 && special_flv <= 3);
        (*tile) = orig + special_flv;
    }

    (*tile) |= flag;
}

void tilep_calc_flags(int parts[], int flag[])
{
    int i;

    for (i = 0; i < TILEP_PARTS_TOTAL; i++)
         flag[i] = TILEP_FLAG_NORMAL;

    if (parts[TILEP_PART_HELM] - 1 >= TILEP_HELM_HELM_OFS)
        flag[TILEP_PART_HAIR] = TILEP_FLAG_HIDE;

    if (parts[TILEP_PART_HELM] - 1 >= TILEP_HELM_FHELM_OFS)
        flag[TILEP_PART_BEARD] = TILEP_FLAG_HIDE;

    if (parts[TILEP_PART_BASE] == TILEP_BASE_NAGA
        || parts[TILEP_PART_BASE] == TILEP_BASE_NAGA + 1)
    {
        flag[TILEP_PART_BOOTS] = flag[TILEP_PART_LEG] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_CUT_NAGA;
    }

    if (parts[TILEP_PART_BASE] == TILEP_BASE_CENTAUR
        || parts[TILEP_PART_BASE] == TILEP_BASE_CENTAUR + 1)
    {
        flag[TILEP_PART_BOOTS] = flag[TILEP_PART_LEG] = TILEP_FLAG_HIDE;
        flag[TILEP_PART_BODY]  = TILEP_FLAG_CUT_CENTAUR;
    }

    if (parts[TILEP_PART_BASE] == TILEP_BASE_MERFOLK_WATER
        || parts[TILEP_PART_BASE] == TILEP_BASE_MERFOLK_WATER + 1)
    {
        flag[TILEP_PART_BOOTS]  = TILEP_FLAG_HIDE;
        flag[TILEP_PART_LEG]    = TILEP_FLAG_HIDE;
        flag[TILEP_PART_SHADOW] = TILEP_FLAG_HIDE;
    }
}

/*
 * Set default parts of each race 
 * body + optional beard, hair, etc 
 */

int draconian_color(int race, int level)
{
    if (level < 0) // hack:monster
    {
        switch(race)
        {
        case MONS_DRACONIAN:        return 0;
        case MONS_BLACK_DRACONIAN:  return 1;
        case MONS_YELLOW_DRACONIAN: return 2;
        case MONS_GREEN_DRACONIAN:  return 4;
        case MONS_MOTTLED_DRACONIAN:return 5;
        case MONS_PALE_DRACONIAN:   return 6;
        case MONS_PURPLE_DRACONIAN: return 7;
        case MONS_RED_DRACONIAN:    return 8;
        case MONS_WHITE_DRACONIAN:  return 9;
        }
    }
    if (level < 7)
        return 0;
    switch(race)
    {
    case SP_BLACK_DRACONIAN:   return 1;
    case SP_GOLDEN_DRACONIAN:  return 2;
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

void tilep_race_default(int race, int gender, int level, int *parts)
{
    int result;
    int hair;
    int beard = 0;

    if (gender == TILEP_GENDER_MALE)
        hair = TILEP_HAIR_SHORT_BLACK;
    else
        hair = TILEP_HAIR_LONG_BLACK;

    switch(race)
    {
        case SP_HUMAN:
            result = TILEP_BASE_HUMAN; 
            break;
        case SP_ELF:
        case SP_HIGH_ELF:
        case SP_GREY_ELF:
        case SP_SLUDGE_ELF:
            result = TILEP_BASE_ELF;
            hair   = TILEP_HAIR_ELF_YELLOW;
            break;
        case SP_DEEP_ELF:
            result = TILEP_BASE_DEEP_ELF;
            hair   = TILEP_HAIR_ELF_WHITE;
            break;
        case SP_HILL_DWARF:
        case SP_MOUNTAIN_DWARF:
            result = TILEP_BASE_DWARF;
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
        case SP_HALFLING:
            result = TILEP_BASE_HALFLING;
            break;
        case SP_HILL_ORC:
            result = TILEP_BASE_ORC;
            hair   = 0;
            break;
        case SP_KOBOLD:
            result = TILEP_BASE_KOBOLD;
            hair   = 0;
            break;
        case SP_MUMMY:
            result = TILEP_BASE_MUMMY;
            hair   = 0;
            break;
        case SP_NAGA:
            result = TILEP_BASE_NAGA;
            break;
        case SP_GNOME:
            result = TILEP_BASE_GNOME;
            break;
        case SP_OGRE:
            result = TILEP_BASE_OGRE;
            break;
        case SP_TROLL:
            result = TILEP_BASE_TROLL;
            hair   = 0;
            break;
        case SP_OGRE_MAGE:
            result = TILEP_BASE_OGRE_MAGE;
            break;

        case SP_BASE_DRACONIAN:
        case SP_RED_DRACONIAN:
        case SP_WHITE_DRACONIAN:
        case SP_GREEN_DRACONIAN:
        case SP_GOLDEN_DRACONIAN:
        case SP_GREY_DRACONIAN:
        case SP_BLACK_DRACONIAN:
        case SP_PURPLE_DRACONIAN:
        case SP_MOTTLED_DRACONIAN:
        case SP_PALE_DRACONIAN:
            hair = 0;
            if (you.mutation[MUT_BIG_WINGS])
                parts[TILEP_PART_DRCWING] = 1 + draconian_color(race, level);

            result = TILEP_BASE_DRACONIAN + draconian_color(race, level)*2;
            break;

        case SP_CENTAUR:
            result = TILEP_BASE_CENTAUR;
            break;
        case SP_DEMIGOD:
            result = TILEP_BASE_DEMIGOD;
            break;
        case SP_SPRIGGAN:
            result = TILEP_BASE_SPRIGGAN;
            break;
        case SP_MINOTAUR:
            result = TILEP_BASE_MINOTAUR;
            hair   = 0;
            break;
        case SP_DEMONSPAWN:
            result = TILEP_BASE_DEMONSPAWN;
            hair   = 0;
            break;
        case SP_GHOUL:
            result = TILEP_BASE_GHOUL;
            hair   = 0;
            break;
        case SP_KENKU:
            result = TILEP_BASE_KENKU;
            break;
        case SP_MERFOLK:
            result = player_in_water() ? TILEP_BASE_MERFOLK_WATER
                                       : TILEP_BASE_MERFOLK;
            break;
        case SP_VAMPIRE:
            result = TILEP_BASE_VAMPIRE;
            if (gender == TILEP_GENDER_MALE)
                hair = TILEP_HAIR_ARAGORN;
            else
                hair = TILEP_HAIR_ARWEN;
            break;
        default:
            result = TILEP_BASE_HUMAN;
            break;
    }

    if (gender == TILEP_GENDER_MALE)
        result++;
    parts[TILEP_PART_BASE]   = result;
    parts[TILEP_PART_HAIR]   = hair;
    parts[TILEP_PART_BEARD]  = beard;
    parts[TILEP_PART_SHADOW] = 1;
}

void tilep_job_default(int job, int gender, int *parts)
{
    parts[TILEP_PART_CLOAK] = 0;
    parts[TILEP_PART_BOOTS] = 0;
    parts[TILEP_PART_LEG]   = 0;
    parts[TILEP_PART_BODY]  = 0;
    parts[TILEP_PART_ARM]   = 0;
    parts[TILEP_PART_HAND1] = 0;
    parts[TILEP_PART_HAND2] = 0;
    parts[TILEP_PART_HELM]  = 0;

    switch(job)
    {
        case JOB_FIGHTER:
            parts[TILEP_PART_BODY]  = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_LEG]   = TILEP_LEG_METAL_SILVER;
            parts[TILEP_PART_HAND2] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            break;

        case JOB_CRUSADER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_WHITE3;
            parts[TILEP_PART_LEG]   = TILEP_LEG_SKIRT_OFS;
            parts[TILEP_PART_HELM]  = TILEP_HELM_HELM_OFS;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_GRAY;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_GRAY;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLUE;
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_HAND2] = TILEP_SHOW_EQUIP;
            break;

        case JOB_PALADIN:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_WHITE;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BROWN;
            parts[TILEP_PART_HELM]  = TILEP_HELM_HELM_OFS;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_GRAY;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_GRAY;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLUE;
            parts[TILEP_PART_HAND2] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            break;

        case JOB_DEATH_KNIGHT:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_BLACK3;
            parts[TILEP_PART_LEG]   = TILEP_LEG_METAL_GRAY;
            parts[TILEP_PART_HELM]  = TILEP_HELM_FHELM_OFS;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_BLACK;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_YELLOW;
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_BLACK;
            break;

        case JOB_CHAOS_KNIGHT:
            parts[TILEP_PART_BODY]  = TILEP_BODY_BELT1;
            parts[TILEP_PART_LEG]   = TILEP_LEG_METAL_GRAY;
            parts[TILEP_PART_HELM]  = TILEP_HELM_FHELM_PLUME;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_HAND2] = TILEP_SHOW_EQUIP;
            break;

        case JOB_BERSERKER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ANIMAL_SKIN;
            parts[TILEP_PART_LEG]   = TILEP_LEG_BELT_REDBROWN;
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_HAND2] = TILEP_SHOW_EQUIP;
            break;

        case JOB_REAVER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_BLACK_GOLD;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BROWN;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_RED_DIM;
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_THIEF:
            parts[TILEP_PART_HELM]  = TILEP_HELM_HOOD_YBROWN;
            parts[TILEP_PART_BODY]  = TILEP_BODY_LEATHER_JACKET;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_SHORT_GRAY;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_SWORD_THIEF;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_WRIST_PURPLE;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_LBROWN;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_BROWN2;
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
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            break;

        case JOB_HEALER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_ROBE_WHITE;
            parts[TILEP_PART_ARM]   = TILEP_ARM_GLOVE_WHITE;
            parts[TILEP_PART_HAND1] = 38;
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
            parts[TILEP_PART_HAND1] = 42;
            parts[TILEP_PART_HAND2] = TILEP_HAND2_BOOK_WHITE;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_SHORT_BROWN;
            parts[TILEP_PART_CLOAK] = TILEP_CLOAK_RED;
            break;

        case JOB_HUNTER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_LEATHER_ARMOUR2;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_BROWN;
            parts[TILEP_PART_HAND1] = TILEP_HAND1_BOW;
            parts[TILEP_PART_ARM]   = 7;
            parts[TILEP_PART_BOOTS] = TILEP_BOOTS_MIDDLE_BROWN;
            break;

        case JOB_GLADIATOR:
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_HAND2] = 10;

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
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_HAND2] = TILEP_SHOW_EQUIP;
            break;

        case JOB_WANDERER:
            parts[TILEP_PART_BODY]  = TILEP_BODY_SHIRT_HAWAII;
            parts[TILEP_PART_LEG]   = TILEP_LEG_PANTS_SHORT_BROWN;
            parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_HAND2] = TILEP_SHOW_EQUIP;
            parts[TILEP_PART_BOOTS] = 11;
            break;
    }
}

/*
 * Patrs index to string
 */
void tilep_part_to_str(int number, char *buf)
{
    //special
    if (number == TILEP_SHOW_EQUIP)
    {
        buf[0] = buf[1] = buf[2] ='*';
    }
    else
    {
        //normal 2 digits
        buf[0] = '0' + (number/100) % 10;
        buf[1] = '0' + (number/ 10) % 10;
        buf[2] = '0' +  number % 10;
    }
    buf[3] ='\0';
}

/*
 * Parts string to index
 */
int tilep_str_to_part(char *str)
{
    //special
    if (str[0] == '*')
        return TILEP_SHOW_EQUIP;
        
    //normal 2 digits
    return atoi(str);
}

const int parts_saved[] ={
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
    -1
};

/*
 * scan input line from dolls.txt
 */
void tilep_scan_parts(char *fbuf, int *parts)
{
    char  ibuf[8];

    int gcount = 0;
    int ccount = 0;
    for (int i = 0; parts_saved[i] != -1; i++)
    {
         int idx;
         ccount = 0;
         int p = parts_saved[i];

         while (fbuf[gcount] != ':' && fbuf[gcount] != '\n'
                && ccount < 4 && gcount < 48)
         {
             ibuf[ccount++] = fbuf[gcount++];
         }

         ibuf[ccount] = '\0';
         gcount++;

         idx = tilep_str_to_part(ibuf);
         if (p == TILEP_PART_BASE)
         {
             int p0 = (parts[p]-1) & (0xfe);
             if (((1-idx) & 1) == 1)
                 p0++;
             parts[p] = p0 + 1;
         }
         else if (idx == TILEP_SHOW_EQUIP)
             parts[p] = TILEP_SHOW_EQUIP;
         else if (idx < 0) // no negative value
             parts[p] = 0;
         else if (idx > tilep_parts_total[p]) // bound it
             parts[p] = tilep_parts_total[p];
         else
             parts[p] = idx;
    }
}

/*
 * format-print parts
 */
void tilep_print_parts(char *fbuf, int *parts)
{
    int i;
    char *ptr = fbuf;
    for (i = 0; parts_saved[i] != -1; i++)
    {
         int p = parts_saved[i];
         if (p == TILEP_PART_BASE) // 0:female  1:male
         {
             sprintf(ptr, "%03d", parts[p]%2);
             ptr += 3;
         }
         else
         {
             tilep_part_to_str(parts[p], ptr);
             ptr += 3;
         }
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
        switch(item.sub_type)
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

    if (is_fixed_artefact( item ))
    {
        switch(item.special)
        {
            case SPWPN_SINGING_SWORD:       return TILEP_HAND1_SINGING_SWORD;
            case SPWPN_WRATH_OF_TROG:       return TILEP_HAND1_AXE_TROG;
            case SPWPN_SCYTHE_OF_CURSES:    return TILEP_HAND1_FINISHER;
            case SPWPN_MACE_OF_VARIABILITY: return TILEP_HAND1_MACE_OF_VARIABILITY;
            case SPWPN_GLAIVE_OF_PRUNE:     return TILEP_HAND1_GLAIVE_OF_PRUNE;
            case SPWPN_SCEPTRE_OF_TORMENT:  return TILEP_HAND1_MACE_RUBY;
            case SPWPN_SWORD_OF_ZONGULDROK: return TILEP_HAND1_ZONGULDROK;
            case SPWPN_SWORD_OF_CEREBOV:    return TILEP_HAND1_SWORD_TWIST;
            case SPWPN_STAFF_OF_DISPATER:   return TILEP_HAND1_DISPATER;
            case SPWPN_SCEPTRE_OF_ASMODEUS: return TILEP_HAND1_ASMODEUS;
            case SPWPN_STAFF_OF_OLGREB:     return TILEP_HAND1_OLGREB;

            case SPWPN_SWORD_OF_POWER: 
            case SPWPN_KNIFE_OF_ACCURACY:
            case SPWPN_VAMPIRES_TOOTH:
            case SPWPN_STAFF_OF_WUCAD_MU:
                break;
        }
    }

    if (is_unrandom_artefact( item ))
    {
        switch (find_unrandart_index(item) + 1)
        {
            // Bloodbane
            case 2:  return TILEP_HAND1_BLOODBANE;
            // Flaming Death
            case 4:  return TILEP_HAND1_FLAMING_DEATH;
            //mace of Brilliance
            case 8:  return TILEP_HAND1_MACE_OF_VARIABILITY;
            //demon blade Leech
            case 12: return TILEP_HAND1_LEECH;
            //dagger of Chilly Death
            case 15: return TILEP_HAND1_CHILLY_DEATH;
            //dagger \"Morg\"
            case 17: return TILEP_HAND1_MORG;
            //scythe \"Finisher
            case 18: return TILEP_HAND1_FINISHER;
            //sling \"Punk
            case 19: return TILEP_HAND1_PUNK;
            //bow of Krishna
            case 20: return TILEP_HAND1_KRISHNA;
            //giant club \"Skullcrusher
            case 22: break;
            //glaive of the Guard
            case 24: break;
            //sword of Jihad
            case 25: return TILEP_HAND1_JIHAD;
            //crossbow \"Fiery Devil
            case 28: return TILEP_HAND1_FIERY_DEVIL;
            //sword of Doom Knight
            case 31: return TILEP_HAND1_DOOM_KNIGHT;
            //Eos
            case 35: break;
            //spear of Voo-Doo
            case 39: return TILEP_HAND1_VOODOO;
            //trident of the Octopus king
            case 40: break;
            //mithril axe \"Arga
            case 42: return TILEP_HAND1_ARGA;
            //Elemental Staff
            case 43: return TILEP_HAND1_ELEMENTAL_STAFF;
            //hand crossbow \"Sniper
            case 44: return TILEP_HAND1_SNIPER;
            //bow \"Erchidel
            case 45: return TILEP_HAND1_GREAT_BOW;
            //plutonium sword
            case 47: return TILEP_HAND1_PLUTONIUM_SWORD;
            //mace \"Undeadhunter
            case 48: return TILEP_HAND1_LARGE_MACE;
        }
    }

    switch (item.sub_type)
    {
    // Blunt
    case WPN_CLUB:              return TILEP_HAND1_CLUB_SLANT;
    case WPN_MACE:              return TILEP_HAND1_MACE;
    case WPN_GREAT_MACE:        return TILEP_HAND1_GREAT_MACE;
    case WPN_FLAIL:             return TILEP_HAND1_FRAIL;
    case WPN_SPIKED_FLAIL:      return TILEP_HAND1_SPIKED_FRAIL;
    case WPN_DIRE_FLAIL:        return TILEP_HAND1_GREAT_FRAIL;
    case WPN_MORNINGSTAR:       return TILEP_HAND1_MORNINGSTAR;
    case WPN_EVENINGSTAR:       return TILEP_HAND1_EVENINGSTAR;
    case WPN_GIANT_CLUB:        return TILEP_HAND1_GIANT_CLUB_SLANT;
    case WPN_GIANT_SPIKED_CLUB: return TILEP_HAND1_GIANT_CLUB_SPIKE_SLANT;
    case WPN_ANCUS:             return TILEP_HAND1_MACE;
    case WPN_WHIP:              return TILEP_HAND1_WHIP;
    case WPN_DEMON_WHIP:        return TILEP_HAND1_BLACK_WHIP;

    // Edge
    case WPN_KNIFE:         return TILEP_HAND1_DAGGER_SLANT;
    case WPN_DAGGER:        return TILEP_HAND1_DAGGER_SLANT;
    case WPN_SHORT_SWORD:   return TILEP_HAND1_SHORT_SWORD_SLANT;
    case WPN_LONG_SWORD:    return TILEP_HAND1_LONG_SWORD_SLANT;
    case WPN_GREAT_SWORD:   return TILEP_HAND1_GREAT_SWORD_SLANT;
    case WPN_SCIMITAR:      return TILEP_HAND1_SCIMITAR;
    case WPN_FALCHION:      return TILEP_HAND1_FALCHION;
    case WPN_SABRE:         return TILEP_HAND1_SABRE;
    case WPN_DEMON_BLADE:   return TILEP_HAND1_SWORD_BLACK;
    case WPN_QUICK_BLADE:   return TILEP_HAND1_DAGGER;
    case WPN_KATANA:        return TILEP_HAND1_KATANA_SLANT;
    case WPN_DOUBLE_SWORD:  return TILEP_HAND1_DOUBLE_SWORD;
    case WPN_TRIPLE_SWORD:  return TILEP_HAND1_TRIPLE_SWORD;
    case WPN_BLESSED_BLADE: return TILEP_HAND1_BLESSED_BLADE;

    // Axe
    case WPN_HAND_AXE:         return TILEP_HAND1_HAND_AXE;
    case WPN_BATTLEAXE:        return TILEP_HAND1_BATTLEAXE;
    case WPN_BROAD_AXE:        return TILEP_HAND1_BROAD_AXE;
    case WPN_WAR_AXE:          return TILEP_HAND1_WAR_AXE;
    case WPN_EXECUTIONERS_AXE: return TILEP_HAND1_EXECUTIONERS_AXE;
    case WPN_BARDICHE:         return TILEP_HAND1_GLAIVE3;

    //Pole
    case WPN_SPEAR:         return TILEP_HAND1_SPEAR;
    case WPN_HALBERD:       return TILEP_HAND1_HALBERD;
    case WPN_GLAIVE:        return TILEP_HAND1_GLAIVE;
    case WPN_QUARTERSTAFF:  return TILEP_HAND1_QUARTERSTAFF1;
    case WPN_SCYTHE:        return TILEP_HAND1_SCYTHE;
    case WPN_HAMMER:        return TILEP_HAND1_HAMMER;
    case WPN_DEMON_TRIDENT: return TILEP_HAND1_DEMON_TRIDENT;
    case WPN_TRIDENT:       return TILEP_HAND1_TRIDENT2;
    case WPN_LAJATANG:      return TILEP_HAND1_D_GLAIVE;

    //Ranged
    case WPN_SLING:         return TILEP_HAND1_SLING;
    case WPN_BOW:           return TILEP_HAND1_BOW2;
    case WPN_CROSSBOW:      return TILEP_HAND1_CROSSBOW;
    case WPN_HAND_CROSSBOW: return TILEP_HAND1_CROSSBOW;
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
        switch (find_unrandart_index(item) + 1)
        {
            // Holy Armour of Zin
            case 6:  return TILEP_BODY_ARMOR_MUMMY;
            // robe of Augmentation
            case 7:  return TILEP_BODY_ROBE_RED2;
            // robe of Misfortune
            case 14: return TILEP_BODY_ARWEN;
            // Lear's chain mail
            case 26: return TILEP_BODY_LEARS_CHAIN_MAIL;
            // skin of Zhor
            case 27: return TILEP_BODY_ZHOR; break;
            // salamander hide armour
            case 29: return TILEP_BODY_LEATHER_RED;
            // robe of Folly
            case 33: return TILEP_BODY_ROBE_BLACK;
            // Edison's patent armour
            case 38: return TILEP_BODY_EDISON;
            // robe of Night
            case 46: return TILEP_BODY_ROBE_OF_NIGHT;
            // armour of the Dragon King
            case 49: break;
        }
    }

    switch (item.sub_type)
    {

    case ARM_ROBE: 
        switch(item.colour)
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
        switch (find_unrandart_index(item) + 1)
        {
            // shield of Ignorance
            case 5:  return TILEP_HAND2_SHIELD_SHAMAN;
            // Bullseye
            case 10: return TILEP_HAND2_BULLSEYE;
            // shield of Resistance
            case 32: return TILEP_HAND2_SHIELD_OF_RESISTANCE;
        }
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
        switch (find_unrandart_index(item) + 1)
        {
            case 11: // crown of Dyrovepreva
                return TILEP_HELM_DYROVEPREVA;
            case 41: // mask of the Dragon
                return TILEP_HELM_ART_DRAGONHELM;
            case 50: // hat of the Alchemist
                return TILEP_HELM_TURBAN_PURPLE;
        }

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
                case THELM_DESC_JEWELLED:
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
        switch (find_unrandart_index(item) + 1)
        {
            case 30: // gauntlets of War (thick brown)
                return TILEP_ARM_GLOVE_BLACK;
            case 51: // fencer's gloves (white)
                return TILEP_ARM_GLOVE_WHITE;
        }
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
        // in anticipation of more unrandart boots
        switch (find_unrandart_index(item) + 1)
        {
            case 23: // boots of the assassin
                return TILEP_BOOTS_MIDDLE_GRAY;
        }
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

    for (i = 0; i < TILEP_PARTS_TOTAL; i++)
    {
         if (strcmp(name, tilep_parts_name[i]) == 0)
             return i;
    }
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

// �p�[�c�̖��O�𐔎��ɕϊ�
int get_parts_idx(int part, char *name)
{
    int res = atoi(name);
    int i;

    for (i = 0; i < tilep_parts_total[part]; i++)
    {
         if (strcmp(name, tilep_comment[ tilep_comment_ofs[part]+i]) == 0)
             return i+1;
    }
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

void tile_init_flavor()
{
    const bool bazaar = is_bazaar();
    const unsigned short baz_col = get_bazaar_special_colour();

    for (int x = 0; x < GXM; x++)
    {
        for (int y = 0; y < GYM; y++)
        {
            int max_wall_flavor  = get_num_wall_flavors() - 1;
            int max_floor_flavor = get_num_floor_flavors() - 1;
            int wall_flavor  = random_range(0, max_wall_flavor);
            int floor_flavor = random_range(0, max_floor_flavor);

            env.tile_flavor[x][y].floor = floor_flavor;
            env.tile_flavor[x][y].wall  = wall_flavor;

            if (grd[x][y] == DNGN_CLOSED_DOOR || grd[x][y] == DNGN_OPEN_DOOR)
            {
                // Check for horizontal gates.

                bool door_left = (x > 0 && grd[x-1][y] == grd[x][y]);
                bool door_right = (x < GXM - 1 && grd[x+1][y] == grd[x][y]);

                if (door_left || door_right)
                {
                    int target;
                    if (door_left && door_right)
                        target = TILE_DNGN_GATE_CLOSED_MIDDLE;
                    else if (door_left)
                        target = TILE_DNGN_GATE_CLOSED_RIGHT;
                    else
                        target = TILE_DNGN_GATE_CLOSED_LEFT;

                    // NOTE: this requires that closed gates and open gates
                    // are positioned in the tile set relative to their
                    // door counterpart.
                    env.tile_flavor[x][y].special =
                        target - TILE_DNGN_CLOSED_DOOR;
                }
                else
                {
                    env.tile_flavor[x][y].special = 0;
                }
            }
            else if (bazaar && env.grid_colours[x][y] == baz_col
                     && grd[x][y] == DNGN_FLOOR)
            {
                int left_grd = (x > 0) ? grd[x-1][y] : DNGN_ROCK_WALL;
                int right_grd = (x < GXM - 1) ? grd[x+1][y] : DNGN_ROCK_WALL;
                int up_grd = (y > 0) ? grd[x][y-1] : DNGN_ROCK_WALL;
                int down_grd = (y < GYM - 1) ? grd[x][y+1] : DNGN_ROCK_WALL;
                unsigned short left_col = (x > 0) ? 
                    env.grid_colours[x-1][y] : BLACK;
                unsigned short right_col = (x < GXM - 1) ? 
                    env.grid_colours[x+1][y] : BLACK;
                unsigned short up_col = (y > 0) ?
                    env.grid_colours[x][y-1] : BLACK;
                unsigned short down_col = (y < GYM - 1) ?
                    env.grid_colours[x][y+1] : BLACK;

                // The special tiles contains part floor and part special, so
                // if there are adjacent floor or special tiles, we should
                // do our best to "connect" them appropriately.  If there are
                // are other tiles there (walls, doors, whatever...) then it
                // doesn't matter.
                bool l_nrm = (left_grd == DNGN_FLOOR && left_col != baz_col);
                bool r_nrm = (right_grd == DNGN_FLOOR && right_col != baz_col);
                bool u_nrm = (up_grd == DNGN_FLOOR && up_col != baz_col);
                bool d_nrm = (down_grd == DNGN_FLOOR && down_col != baz_col);

                bool l_spc = (left_grd == DNGN_FLOOR && left_col == baz_col);
                bool r_spc = (right_grd == DNGN_FLOOR && right_col == baz_col);
                bool u_spc = (up_grd == DNGN_FLOOR && up_col == baz_col);
                bool d_spc = (down_grd == DNGN_FLOOR && down_col == baz_col);

                if (l_nrm && r_nrm || u_nrm && d_nrm)
                {
                    // Not much to do here...
                    env.tile_flavor[x][y].special = SPECIAL_FULL;
                }
                else if (l_nrm)
                {
                    if (u_nrm)
                        env.tile_flavor[x][y].special = SPECIAL_NW;
                    else if (d_nrm)
                        env.tile_flavor[x][y].special = SPECIAL_SW;
                    else if (u_spc && d_spc)
                        env.tile_flavor[x][y].special = SPECIAL_W;
                    else if (u_spc && r_spc)
                        env.tile_flavor[x][y].special = SPECIAL_SW;
                    else if (d_spc && r_spc)
                        env.tile_flavor[x][y].special = SPECIAL_NW;
                    else if (u_spc)
                        env.tile_flavor[x][y].special = coinflip() ?
                            SPECIAL_W : SPECIAL_SW;
                    else if (d_spc)
                        env.tile_flavor[x][y].special = coinflip() ?
                            SPECIAL_W : SPECIAL_NW;
                    else
                        env.tile_flavor[x][y].special = jitter(SPECIAL_W);
                }
                else if (r_nrm)
                {
                    if (u_nrm)
                        env.tile_flavor[x][y].special = SPECIAL_NE;
                    else if (d_nrm)
                        env.tile_flavor[x][y].special = SPECIAL_SE;
                    else if (u_spc && d_spc)
                        env.tile_flavor[x][y].special = SPECIAL_E;
                    else if (u_spc && l_spc)
                        env.tile_flavor[x][y].special = SPECIAL_SE;
                    else if (d_spc && l_spc)
                        env.tile_flavor[x][y].special = SPECIAL_NE;
                    else if (u_spc)
                        env.tile_flavor[x][y].special = coinflip() ?
                            SPECIAL_E : SPECIAL_SE;
                    else if (d_spc)
                        env.tile_flavor[x][y].special = coinflip() ?
                            SPECIAL_E : SPECIAL_NE;
                    else
                        env.tile_flavor[x][y].special = jitter(SPECIAL_E);
                }
                else if (u_nrm)
                {
                    if (r_spc && l_spc)
                        env.tile_flavor[x][y].special = SPECIAL_N;
                    else if (r_spc && d_spc)
                        env.tile_flavor[x][y].special = SPECIAL_NW;
                    else if (l_spc && d_spc)
                        env.tile_flavor[x][y].special = SPECIAL_NE;
                    else if (r_spc)
                        env.tile_flavor[x][y].special = coinflip() ?
                            SPECIAL_N : SPECIAL_NW;
                    else if (l_spc)
                        env.tile_flavor[x][y].special = coinflip() ?
                            SPECIAL_N : SPECIAL_NE;
                    else
                        env.tile_flavor[x][y].special = jitter(SPECIAL_N);
                }
                else if (d_nrm)
                {
                    if (r_spc && l_spc)
                        env.tile_flavor[x][y].special = SPECIAL_S;
                    else if (r_spc && u_spc)
                        env.tile_flavor[x][y].special = SPECIAL_SW;
                    else if (l_spc && u_spc)
                        env.tile_flavor[x][y].special = SPECIAL_SE;
                    else if (r_spc)
                        env.tile_flavor[x][y].special = coinflip() ?
                            SPECIAL_S : SPECIAL_SW;
                    else if (l_spc)
                        env.tile_flavor[x][y].special = coinflip() ?
                            SPECIAL_S : SPECIAL_SE;
                    else
                        env.tile_flavor[x][y].special = jitter(SPECIAL_S);
                }
                else if (u_spc && d_spc)
                {
                    // We know this value is already initialized and
                    // is necessarily in bounds.
                    char t = env.tile_flavor[x][y-1].special;
                    if (t == SPECIAL_NE || t == SPECIAL_E)
                        env.tile_flavor[x][y].special = SPECIAL_E;
                    else if (t == SPECIAL_NW || t == SPECIAL_W)
                        env.tile_flavor[x][y].special = SPECIAL_W;
                    else
                        env.tile_flavor[x][y].special = SPECIAL_FULL;
                }
                else if (r_spc && l_spc)
                {
                    // We know this value is already initialized and
                    // is necessarily in bounds.
                    char t = env.tile_flavor[x-1][y].special;
                    if (t == SPECIAL_NW || t == SPECIAL_N)
                        env.tile_flavor[x][y].special = SPECIAL_N;
                    else if (t == SPECIAL_SW || t == SPECIAL_S)
                        env.tile_flavor[x][y].special = SPECIAL_S;
                    else
                        env.tile_flavor[x][y].special = SPECIAL_FULL;
                }
                else if (u_spc && l_spc)
                {
                    env.tile_flavor[x][y].special = SPECIAL_SE;
                }
                else if (u_spc && r_spc)
                {
                    env.tile_flavor[x][y].special = SPECIAL_SW;
                }
                else if (d_spc && l_spc)
                {
                    env.tile_flavor[x][y].special = SPECIAL_NE;
                }
                else if (d_spc && r_spc)
                {
                    env.tile_flavor[x][y].special = SPECIAL_NW;
                }
                else
                {
                    env.tile_flavor[x][y].special = SPECIAL_FULL;
                }
            }
            else
            {
                env.tile_flavor[x][y].special = 0;
            }
        }
    }

    if (!bazaar)
        return;

    // Second pass for clean up.  The only bad part about the above
    // algorithm is that it could turn a block of floor like this:
    //
    // N4NN
    // 3125
    // NN6N
    //
    // (KEY: N = normal floor, # = special floor)
    //
    // Into these flavors:
    // 1 - SPECIAL_S
    // 2 - SPECIAL_N
    // 3-6, not important
    //
    // Generally the tiles don't fit with a north to the right or left
    // of a south tile.  What we really want to do is to separate the
    // two regions, by making 1 a SPECIAL_SE and 2 a SPECIAL_NW tile.
    for (int x = 0; x < GXM - 1; x++)
    {
        for (int y = 0; y < GYM - 1; y++)
        {
            if (grd[x][y] != DNGN_FLOOR || env.grid_colours[x][y] != baz_col)
                continue;

            if (env.tile_flavor[x][y].special != SPECIAL_N &&
                env.tile_flavor[x][y].special != SPECIAL_S &&
                env.tile_flavor[x][y].special != SPECIAL_E &&
                env.tile_flavor[x][y].special != SPECIAL_W)
            {
                continue;
            }

            int right_flavor = x < GXM - 1 ? env.tile_flavor[x+1][y].special :
                SPECIAL_FULL;
            int down_flavor = y < GYM - 1 ? env.tile_flavor[x][y+1].special :
                SPECIAL_FULL;
            int this_flavor = env.tile_flavor[x][y].special;

            if (this_flavor == SPECIAL_N && right_flavor == SPECIAL_S)
            {
                env.tile_flavor[x][y].special = SPECIAL_NE;
                env.tile_flavor[x+1][y].special = SPECIAL_SW;
            }
            else if (this_flavor == SPECIAL_S && right_flavor == SPECIAL_N)
            {
                env.tile_flavor[x][y].special = SPECIAL_SE;
                env.tile_flavor[x+1][y].special = SPECIAL_NW;
            }
            else if (this_flavor == SPECIAL_E && down_flavor == SPECIAL_W)
            {
                env.tile_flavor[x][y].special = SPECIAL_SE;
                env.tile_flavor[x][y+1].special = SPECIAL_NW;
            }
            else if (this_flavor == SPECIAL_W && down_flavor == SPECIAL_E)
            {
                env.tile_flavor[x][y].special = SPECIAL_NE;
                env.tile_flavor[x][y+1].special = SPECIAL_SW;
            }
        }
    }
}

void tile_clear_buf()
{
    for (int y = 0; y < GYM; y++)
    {
        for (int x = 0; x < GXM; x++)
        {
            tile_dngn[x][y] = TILE_DNGN_UNSEEN;
        }
    }
}

/* called from view.cc */
void tile_draw_floor()
{
    int cx, cy;
    for (cy = 0; cy < env.tile_fg.height(); cy++)
    {
        for (cx = 0; cx < env.tile_fg.width(); cx++)
        {
            const coord_def ep(cx+1, cy+1);
            const coord_def gc = view2grid(show2view(ep));
            int bg = TILE_DNGN_UNSEEN | tile_unseen_flag(gc);

            int object = env.show(ep);
            if (in_bounds(gc) && object != 0)
            {
                if (object == DNGN_SECRET_DOOR)
                    object = (int)grid_secret_door_appearance(gc.x, gc.y);

                tile_dngn[gc.x][gc.y] = tileidx_feature(object, gc.x, gc.y);

                if (is_travelable_stair((dungeon_feature_type)object)
                    && !travel_cache.know_stair(gc))
                {
                    tile_dngn[gc.x][gc.y] |= TILE_FLAG_NEW_STAIR;
                }
                bg = tile_dngn[gc.x][gc.y];
            }
            // init tiles
            env.tile_bg[ep.x-1][ep.y-1] = bg;
            env.tile_fg[ep.x-1][ep.y-1] = 0;
        }
    }
}

// Called from item() in view.cc
void tile_place_item(int x, int y, int idx)
{
    int t = tileidx_item(mitm[idx]);
    if (mitm[idx].link != NON_ITEM)
        t |= TILE_FLAG_S_UNDER;
        
    env.tile_fg[x-1][y-1] = t;

    if (item_needs_autopickup(mitm[idx]))
        env.tile_bg[x-1][y-1] |= TILE_FLAG_CURSOR3;
}

void tile_place_item_bk(int gx, int gy, int idx)
{
    int t = tileidx_item(mitm[idx]);
    env.tile_bk_fg[gx][gy] = t;

    if (item_needs_autopickup(mitm[idx]))
        env.tile_bk_bg[gx][gy] |= TILE_FLAG_CURSOR3;
}

void tile_place_tile_bk(int gx, int gy, int tileidx)
{
    env.tile_bk_fg[gx][gy] = tileidx;
}

// Called from item() in view.cc
void tile_place_item_marker(int x, int y, int idx)
{
    env.tile_fg[x-1][y-1] |= TILE_FLAG_S_UNDER;

    if (item_needs_autopickup(mitm[idx]))
        env.tile_bg[x-1][y-1] |= TILE_FLAG_CURSOR3;
}

// Called from monster_grid() in view.cc
void tile_place_monster(int gx, int gy, int idx, bool foreground, bool detected)
{
    if (idx == NON_MONSTER)
    {
        return;
    }
    
    const coord_def gc(gx, gy);
    const coord_def ep = view2show(grid2view(gc));

    int t  = _tileidx_monster(idx, detected);
    int t0 = t & TILE_FLAG_MASK;
    int flag = t & (~TILE_FLAG_MASK);
    int mon_wep = menv[idx].inv[MSLOT_WEAPON];

    if (mons_is_mimic(menv[idx].type))
    {
        const monsters *mon = &menv[idx];
        if (!mons_is_known_mimic(mon))
        {
            // if necessary add item brand
            if (igrd[gx][gy] != NON_ITEM)
            {
                if (foreground)
                    t |= TILE_FLAG_S_UNDER;
                else
                    t0 |= TILE_FLAG_S_UNDER;
            }

            item_def item;
            get_mimic_item( mon, item );
            if (item_needs_autopickup(item))
            {
                if (foreground)
                    env.tile_bg[ep.x-1][ep.y-1] |= TILE_FLAG_CURSOR3;
                else
                    env.tile_bk_bg[gx][gy] |= TILE_FLAG_CURSOR3;
            }
        }
    }
    else if (menv[idx].holiness() == MH_PLANT)
    {
        // if necessary add item brand
        if (igrd[gx][gy] != NON_ITEM)
        {
            if (foreground)
                t |= TILE_FLAG_S_UNDER;
            else
                t0 |= TILE_FLAG_S_UNDER;
        }
    }
    else if (menv[idx].type >=  MONS_DRACONIAN &&
             menv[idx].type <= MONS_DRACONIAN_SCORCHER)
    {
        int race = draco_subspecies(&menv[idx]);
        int cls = menv[idx].type;
        int eq = 0;
        if (mon_wep != NON_ITEM &&
            (cls == race || cls == MONS_DRACONIAN_KNIGHT))
        {
            eq = tilep_equ_weapon(mitm[mon_wep]);
        }
        t = flag | TileMcacheFind(cls, eq, race);
    }
    else if (mon_wep != NON_ITEM)
    {
        int eq = tilep_equ_weapon(mitm[mon_wep]);
        switch(t0)
        {
            // 3D chars
            case TILE_MONS_VAULT_GUARD:

            case TILE_MONS_BLORK_THE_ORC:
            case TILE_MONS_ORC:
            case TILE_MONS_ORC_KNIGHT:
            case TILE_MONS_ORC_WARLORD:
            case TILE_MONS_ORC_WARRIOR:
            case TILE_MONS_URUG:

            case TILE_MONS_GOBLIN:
            case TILE_MONS_IJYB:
            case TILE_MONS_HOBGOBLIN:
            case TILE_MONS_GNOLL:
            case TILE_MONS_BOGGART:

            case TILE_MONS_KOBOLD:
            case TILE_MONS_KOBOLD_DEMONOLOGIST:
            case TILE_MONS_BIG_KOBOLD:

            case TILE_MONS_DEEP_ELF_FIGHTER:
            case TILE_MONS_DEEP_ELF_SOLDIER:
            case TILE_MONS_DEEP_ELF_KNIGHT:
            case TILE_MONS_DEEP_ELF_BLADEMASTER:
            case TILE_MONS_DEEP_ELF_MASTER_ARCHER:
            case TILE_MONS_DEEP_ELF_MAGE:
            case TILE_MONS_DEEP_ELF_SUMMONER:
            case TILE_MONS_DEEP_ELF_CONJURER:
            case TILE_MONS_DEEP_ELF_PRIEST:
            case TILE_MONS_DEEP_ELF_HIGH_PRIEST:
            case TILE_MONS_DEEP_ELF_DEMONOLOGIST:
            case TILE_MONS_DEEP_ELF_ANNIHILATOR:
            case TILE_MONS_DEEP_ELF_SORCERER:
            case TILE_MONS_DEEP_ELF_DEATH_MAGE:

            case TILE_MONS_MIDGE:
            case TILE_MONS_IMP:

            case TILE_MONS_NAGA:
            case TILE_MONS_GREATER_NAGA:
            case TILE_MONS_NAGA_WARRIOR:
            case TILE_MONS_NAGA_MAGE:

            case TILE_MONS_OGRE_MAGE:
            case TILE_MONS_RED_DEVIL:
            case TILE_MONS_WIZARD:
            case TILE_MONS_HUMAN:
            case TILE_MONS_ELF:

            case TILE_MONS_ANGEL:

            case TILE_MONS_HELL_KNIGHT:

            case TILE_MONS_NORRIS:
            case TILE_MONS_MAUD:
            case TILE_MONS_DUANE:
            case TILE_MONS_EDMUND:
            case TILE_MONS_FRANCES:
            case TILE_MONS_HAROLD:
            case TILE_MONS_JOSEPH:
            case TILE_MONS_JOZEF:
            case TILE_MONS_RUPERT:
            case TILE_MONS_TERENCE:
            case TILE_MONS_WAYNE:
            case TILE_MONS_FREDERICK:

            case TILE_MONS_RAKSHASA:
            case TILE_MONS_RAKSHASA_FAKE:

            case TILE_MONS_VAMPIRE_KNIGHT:

            case TILE_MONS_SKELETAL_WARRIOR:
            case TILE_MONS_MERMAID:
            case TILE_MONS_MERMAID_WATER:
            case TILE_MONS_MERFOLK_FIGHTER:
            case TILE_MONS_MERFOLK_FIGHTER_WATER:

            if (eq != 0 )
                t = flag | TileMcacheFind(t0, eq);
            break;

        }
    }

    if (foreground)
    {
        env.tile_fg[ep.x-1][ep.y-1] = t;
    }
    else
    {
        env.tile_bk_fg[gc.x][gc.y] = t0;
    }
}

void tile_place_cloud(int x, int y, int type, int decay)
{
    env.tile_fg[x-1][y-1] = _tileidx_cloud(type, decay);
}

unsigned int tileRayCount = 0;
FixedVector<coord_def, 30> tileRays;

void tile_place_ray(const coord_def& gc)
{
    // Record rays for later.  The curses version just applies
    // rays directly to the screen.  The tiles version doesn't have
    // (nor want) such direct access.  So, it batches up all of the
    // rays and applies them in viewwindow(...).
    if (tileRayCount < tileRays.size() - 1)
    {
        tileRays[tileRayCount++] = view2show(grid2view(gc));
    }
}

void tile_draw_rays(bool resetCount)
{
    for (unsigned int i = 0; i < tileRayCount; i++)
    {
        env.tile_bg[tileRays[i].x-1][tileRays[i].y-1] |= TILE_FLAG_RAY;
    }

    if (resetCount)
        tileRayCount = 0;
}

void tile_finish_dngn(unsigned int *tileb, int cx, int cy)
{
    int x, y;
    int count = 0;

    const bool bazaar = is_bazaar();
    const unsigned short baz_col = get_bazaar_special_colour();

    for (y = 0; y < crawl_view.viewsz.y; y++)
    {
        for (x = 0; x < crawl_view.viewsz.x; x++)
        {
            // View coords are not centered on you, but on (cx,cy)
            const int gx = view2gridX(x + 1) + cx - you.x_pos;
            const int gy = view2gridY(y + 1) + cy - you.y_pos;

            char wall_flv    = 0;
            char floor_flv   = 0;
            char special_flv = 0;
            bool is_special  = false;
            const bool in_bounds = (map_bounds(gx, gy));

            if (in_bounds)
            {
                wall_flv    = env.tile_flavor[gx][gy].wall;
                floor_flv   = env.tile_flavor[gx][gy].floor;
                special_flv = env.tile_flavor[gx][gy].special;
                is_special  = (bazaar && env.grid_colours[gx][gy] == baz_col);
            }

            _finalize_tile(&tileb[count], is_special,
                           wall_flv, floor_flv, special_flv);
            _finalize_tile(&tileb[count+1], is_special,
                           wall_flv, floor_flv, special_flv);

            const coord_def gc(gx, gy);
            if (is_excluded(gc))
            {
                tileb[count+1] |= TILE_FLAG_TRAVEL_EX;
            }

            if (in_bounds)
            {
                if (is_bloodcovered(gx, gy))
                {
                    tileb[count+1] |= TILE_FLAG_BLOOD;
                }

                if (is_sanctuary(gx, gy))
                {
                    tileb[count+1] |= TILE_FLAG_SANCTUARY;
                }
            }

            count += 2;
        }
    }
}

void tile_draw_dungeon(unsigned int *tileb)
{
    tile_finish_dngn(tileb, you.x_pos, you.y_pos);
    TileDrawDungeon(tileb);    
}

#define swapint(a, b) {int tmp = a; a = b; b = tmp;}

// Item is unided(1) or tried(2) or id'ed (0)
static int _item_unid_type(const item_def &item)
{
    if ((item.flags & ISFLAG_KNOW_TYPE) != 0)
        return 0;

    const int s = item.sub_type;
    const id_arr& id = get_typeid_array();
    int id0 = 0;

    switch (item.base_type)
    {
        case OBJ_STAVES:
          id0 = id[ IDTYPE_STAVES ][s];
          if (id0 != ID_KNOWN_TYPE)
             return 1;
          else
             return 0;
             
        case OBJ_SCROLLS:
          id0 = id[ IDTYPE_SCROLLS ][s];
          break;

        case OBJ_WANDS:
          id0 = id[ IDTYPE_WANDS ][s];
          break;

        case OBJ_POTIONS:
          id0 = id[ IDTYPE_POTIONS ][s];
          break;

        case OBJ_JEWELLERY:
          id0 = id[ IDTYPE_JEWELLERY ][s];
          break;
          
        default:
          return 0;
    }
    
    if (id0 == ID_TRIED_TYPE)
        return 2;
    else if (id0 != ID_KNOWN_TYPE)
        return 1;

    return 0;
}

// Helper routine: sort floor item index and pack into idx
static int _pack_floor_item(int *idx, int *flag, int *isort, int max)
{
    int n = 0;
    static int isort_weapon2[NUM_WEAPONS];
    static int isort_armour2[NUM_ARMOURS];

    static const int isort_weapon[NUM_WEAPONS] = 
    {
        WPN_WHIP, WPN_CLUB, WPN_HAMMER, WPN_MACE,
        WPN_FLAIL, WPN_DEMON_WHIP,
        WPN_ANCUS, WPN_MORNINGSTAR, WPN_EVENINGSTAR,
        WPN_SPIKED_FLAIL, WPN_GREAT_MACE, WPN_DIRE_FLAIL,
        WPN_GIANT_CLUB,  WPN_GIANT_SPIKED_CLUB,

        WPN_KNIFE, WPN_DAGGER, WPN_SHORT_SWORD, WPN_SABRE, WPN_QUICK_BLADE,
        WPN_FALCHION, WPN_LONG_SWORD, WPN_SCIMITAR, WPN_KATANA,
        WPN_DEMON_BLADE, WPN_DOUBLE_SWORD, WPN_GREAT_SWORD, WPN_TRIPLE_SWORD,

        WPN_HAND_AXE, WPN_WAR_AXE, WPN_BROAD_AXE,
        WPN_BATTLEAXE, WPN_EXECUTIONERS_AXE,

        WPN_SPEAR, WPN_TRIDENT, WPN_HALBERD, WPN_SCYTHE,
        WPN_GLAIVE, WPN_DEMON_TRIDENT,

        WPN_QUARTERSTAFF,

        WPN_SLING, WPN_BOW, WPN_CROSSBOW, WPN_HAND_CROSSBOW
    };
    static const int isort_armour[NUM_ARMOURS] = 
    {
        ARM_ROBE, 
        ARM_ANIMAL_SKIN,
        ARM_LEATHER_ARMOUR,
        ARM_TROLL_LEATHER_ARMOUR,
        ARM_RING_MAIL, ARM_SCALE_MAIL, ARM_CHAIN_MAIL, 
        ARM_SPLINT_MAIL, ARM_BANDED_MAIL, ARM_PLATE_MAIL,
        ARM_CRYSTAL_PLATE_MAIL,
        ARM_SWAMP_DRAGON_ARMOUR,
        ARM_MOTTLED_DRAGON_ARMOUR,
        ARM_STEAM_DRAGON_ARMOUR,
        ARM_DRAGON_ARMOUR,
        ARM_ICE_DRAGON_ARMOUR,
        ARM_STORM_DRAGON_ARMOUR,
        ARM_GOLD_DRAGON_ARMOUR,
        ARM_TROLL_HIDE,
        ARM_SWAMP_DRAGON_HIDE,
        ARM_MOTTLED_DRAGON_HIDE,
        ARM_STEAM_DRAGON_HIDE,
        ARM_DRAGON_HIDE,
        ARM_ICE_DRAGON_HIDE,
        ARM_STORM_DRAGON_HIDE,
        ARM_GOLD_DRAGON_HIDE,
        ARM_CLOAK,
        ARM_BUCKLER, ARM_SHIELD, ARM_LARGE_SHIELD,
        ARM_HELMET, ARM_GLOVES, ARM_BOOTS
    };

    for (int i = 0; i < NUM_WEAPONS; i++)
         isort_weapon2[isort_weapon[i]] = i;
         
    for (int i = 0; i < NUM_ARMOURS; i++)
         isort_armour2[isort_armour[i]] = i;

    int o = igrd[you.x_pos][you.y_pos];
    if (o == NON_ITEM) return 0;

    while (o != NON_ITEM)
    {
        int id0  = _item_unid_type(mitm[o]);
        int next = mitm[o].link;
        int typ  = mitm[o].base_type;

        if (n >= max) break;

        idx[n] = o;
        isort[n] = typ * 256 * 3;
        if (typ == OBJ_WEAPONS)
        {
            isort[n] += 3 * isort_weapon2[ mitm[o].sub_type];
        }
        if (typ == OBJ_ARMOUR)
        {
            isort[n] += 3 * isort_armour2[ mitm[o].sub_type ];
        }
        flag[n] = 0;

        if (item_ident( mitm[o], ISFLAG_KNOW_CURSE ) &&
            item_cursed(mitm[o]))
        {
            flag[n] |= TILEI_FLAG_CURSE;
        }

        if (id0 != 0)
        {
            isort[n] += id0;
            if (id0 == 2)
                flag[n] = TILEI_FLAG_TRIED;
        }
        flag[n] |= TILEI_FLAG_FLOOR;

        // Simple Bubble sort
        int k = n;
        while (k > 0 && isort[k-1] > isort[k])
        {
            swapint(idx[k-1], idx[k]);
            swapint(isort[k-1], isort[k]);
            swapint(flag[k-1], flag[k]);
            k--;
        }
        n++;
        o = next;
    }
    return n;
}

// Helper routine: Calculate tile index and quantity data to be displayed
static void _finish_inven_data(int n, int *tiles, int *num, int *idx,
                               int *iflag)
{
    int i;

    for (i = 0; i < n; i++)
    {
        int q = -1;
        int j = idx[i];
        item_def *itm;

        if (j == -1)
        {
            num[i]   = -1;
            tiles[i] = 0;
            continue;
        }

        if (iflag[i] & TILEI_FLAG_FLOOR)
            itm = &mitm[j];
        else
            itm = &you.inv[j];

        int type = itm->base_type;

        if (type == OBJ_FOOD || type == OBJ_SCROLLS
            || type == OBJ_POTIONS || type == OBJ_MISSILES)
        {
            q = itm->quantity;
        }
        if (q == 1)
            q = -1;

        if ( type == OBJ_WANDS
             && ((itm->flags & ISFLAG_KNOW_PLUSES )
                 || itm->plus2 == ZAPCOUNT_EMPTY) )
        {
            q = itm->plus;
        }

        tiles[i] = tileidx_item(*itm);
        num[i] = q;
    }
}

// Display Inventory/floor items
#include "guic.h"
extern TileRegionClass *region_item;
extern TileRegionClass *region_item2;
extern WinClass *win_main;

void tile_draw_inv(int flag)
{
    // "inventory" including items on floor
    #define MAXINV 200
    int tiles[MAXINV];
    int num[MAXINV];
    int idx[MAXINV];
    int iflag[MAXINV];
    int isort[MAXINV];

    if (flag == -1)
    {
        flag = (win_main->active_layer == 0) ? REGION_INV1
                                             : REGION_INV2;
    }

    TileRegionClass *r = (flag == REGION_INV1) ? region_item
                                               : region_item2;
    int numInvTiles = r->mx * r->my;
    if (numInvTiles > MAXINV)
        numInvTiles = MAXINV;

    // which items to show in inventory
    const char *item_chars = Options.tile_show_items;

    // show no items, please
    if (item_chars[0] == 0)
        return;

    int eq_flag[ENDOFPACK];
    int empty = 0; // counts empty slots

    // first set eq_flag = 1 for all slots that actually hold valid items
    // XXX: Why? --jpeg
    for (int i = 0; i < ENDOFPACK; i++)
    {
        eq_flag[i] =
            (you.inv[i].quantity != 0 && is_valid_item( you.inv[i])) ? 1 : 0;

        if (!eq_flag[i])
            empty++;
    }

    // next, increase eq_flag to 2 if it's actually equipped
    // FIX ME: Doesn't check for sensible equipment, i.e.
    //         wielded armour counts, too
    for (int eq = 0; eq < NUM_EQUIP; eq++)
    {
        int slot = you.equip[eq];
        if (slot >= 0 && slot < ENDOFPACK)
            eq_flag[slot] = 2;
    }

    int n = 0;
    
    // item.base_type <-> char conversion table
    const static char *obj_syms = ")([/%#?=!#+\\0}x";
    
    for (int i = 0; i < (int)strlen(item_chars); i++)
    {
        int top = n;
        char ic = item_chars[i];

        if (n >= numInvTiles)
            break;

        // Items on the floor
        if (ic == '.')
        {
            n += _pack_floor_item(&idx[n], &iflag[n], &isort[n],
                                  numInvTiles - n);
            continue;
        }

        // empty slots
        if (ic == '_')
        {
            for (int j = 0; j < empty && n < numInvTiles; j++)
            {
                idx[n] = -1;
                iflag[n] = 0;
                n++;
            }
            continue;
        }

        // convert item char to item type
        int type = -1;
        for (int j = 0; j < (int)strlen(obj_syms); j++)
        {
            if (obj_syms[j] == ic)
            {
                type = j;
                break;
            }
        }

        if (type == -1)
            continue;

        for (int j = 0; j < ENDOFPACK && n < numInvTiles; j++)
        {
            if (you.inv[j].base_type == type && eq_flag[j] != 0)
            {
                int sval = NUM_EQUIP + you.inv[j].sub_type;
                int base = 0;
                int id0  = _item_unid_type(you.inv[j]);

                idx[n] = j;
                iflag[n] = 0;

                if (type == OBJ_JEWELLERY && sval >= AMU_RAGE)
                {
                    base = 1000;
                    sval = base + sval;
                }

                if (id0 == 2)
                {
                     iflag[n] |= TILEI_FLAG_TRIED;
                     // To the tail
                     sval = base + 980;
                }
                else if (id0 == 1)
                {
                     // To the tail
                     sval = base + 990;
                }

                // Equipment first
                if (eq_flag[j] == 2)
                {
                     //sval = base;
                     iflag[n] |= TILEI_FLAG_EQUIP;
                }

                if (item_cursed(you.inv[j])
                    && item_ident( you.inv[j], ISFLAG_KNOW_CURSE ))
                {
                    iflag[n] |= TILEI_FLAG_CURSE;
                }

                if (flag == REGION_INV2)
                    sval = j;

                isort[n] = sval;

                int k = n;
                while (k > top && isort[k-1] > isort[k])
                {
                    swapint(idx[k-1], idx[k]);
                    swapint(isort[k-1], isort[k]);
                    swapint(iflag[k-1], iflag[k]);
                    k--;
                }

                n++;
            } // type == base
        } // j
    }  // i

    _finish_inven_data(n, tiles, num, idx, iflag);

    for (int i = n; i < numInvTiles; i++)
    {
         tiles[i] = 0;
         num[i]   = 0;
         idx[i]   = -1;
         iflag[i] = 0;
    }
    TileDrawInvData(n, flag, tiles, num, idx, iflag);
}

#endif

/*
 *  File:       mon-pick.cc
 *  Summary:    Functions used to help determine which monsters should appear.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <2>   08-Mar-2000      DLB             enumeration & clean-up
 *               <1>     -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "mon-pick.h"

#include "externs.h"

static int mons_cocytus_level(int mcls);
static int mons_cocytus_rare(int mcls);
static int mons_crypt_level(int mcls);
static int mons_crypt_rare(int mcls);
static int mons_dis_level(int mcls);
static int mons_dis_rare(int mcls);
static int mons_gehenna_level(int mcls);
static int mons_gehenna_rare(int mcls);
static int mons_hallblade_level(int mcls);
static int mons_hallblade_rare(int mcls);
static int mons_hallelf_level(int mcls);
static int mons_hallelf_rare(int mcls);
static int mons_hallzot_level(int mcls);
static int mons_hallzot_rare(int mcls);
static int mons_hive_level(int mcls);
static int mons_hive_rare(int mcls);
static int mons_lair_level(int mcls);
static int mons_lair_rare(int mcls);
static int mons_mineorc_level(int mcls);
static int mons_mineorc_rare(int mcls);
static int mons_pitslime_level(int mcls);
static int mons_pitslime_rare(int mcls);
static int mons_pitsnake_level(int mcls);
static int mons_pitsnake_rare(int mcls);
static int mons_standard_level(int mcls);
static int mons_standard_rare(int mcls);
static int mons_swamp_level(int mcls);
static int mons_swamp_rare(int mcls);
static int mons_tartarus_level(int mcls);
static int mons_tartarus_rare(int mcls);
static int mons_tomb_level(int mcls);
static int mons_tomb_rare(int mcls);

/* ******************* BEGIN EXTERNAL FUNCTIONS ******************* */
int branch_depth(int branch)
{
    switch (branch)
    {
    case STAIRS_LAIR:
        return 10;

    case STAIRS_VAULTS:
        return 8;

    case STAIRS_ELVEN_HALLS:
        return 7;

    case STAIRS_SLIME_PITS:
        return 6;

    case STAIRS_CRYPT:
    case STAIRS_HALL_OF_ZOT:
    case STAIRS_SNAKE_PIT:
    case STAIRS_SWAMP:
        return 5;

    case STAIRS_HIVE:
    case STAIRS_ORCISH_MINES:
        return 4;

    case STAIRS_TOMB:
        return 3;

    case STAIRS_ECUMENICAL_TEMPLE:
    case STAIRS_HALL_OF_BLADES:
        return 1;

    default:
        return 0;
    }
}                               // end branch_depth()

// NB - When adding new branches or levels above 50, you must
// change pre-game deletion routine in new_game in newgame.cc

int mons_level(int mcls)
{
    int monster_level = 0;
    int (*fnc_level) (int) = 0;

    if (you.level_type == LEVEL_ABYSS)
        monster_level = ((mons_abyss(mcls)) ? 51 : 0);
    else if (you.level_type == LEVEL_PANDEMONIUM)
        monster_level = ((mons_pan(mcls)) ? 52 : 0);
    else
    {
        fnc_level =
            ((you.where_are_you == BRANCH_DIS)          ? mons_dis_level :
             (you.where_are_you == BRANCH_GEHENNA)      ? mons_gehenna_level :
             (you.where_are_you == BRANCH_COCYTUS)      ? mons_cocytus_level :
             (you.where_are_you == BRANCH_TARTARUS)     ? mons_tartarus_level :
             (you.where_are_you == BRANCH_ORCISH_MINES) ? mons_mineorc_level :
             (you.where_are_you == BRANCH_HIVE)         ? mons_hive_level :
             (you.where_are_you == BRANCH_LAIR)         ? mons_lair_level :
             (you.where_are_you == BRANCH_SLIME_PITS)   ? mons_pitslime_level :
             (you.where_are_you == BRANCH_CRYPT)        ? mons_crypt_level :
             (you.where_are_you == BRANCH_HALL_OF_BLADES)?mons_hallblade_level :
             (you.where_are_you == BRANCH_HALL_OF_ZOT)  ? mons_hallzot_level :
             (you.where_are_you == BRANCH_SNAKE_PIT)    ? mons_pitsnake_level :
             (you.where_are_you == BRANCH_ELVEN_HALLS)  ? mons_hallelf_level :
             (you.where_are_you == BRANCH_TOMB)         ? mons_tomb_level :
             (you.where_are_you == BRANCH_SWAMP)        ? mons_swamp_level :
             (you.where_are_you == BRANCH_VAULTS)       ? mons_standard_level
                                                        : mons_standard_level);

        monster_level = fnc_level(mcls);
    }

    return (monster_level);
}                               // end mons_level()

// higher values returned means the monster is "more common"
// a return value of zero means the monster will never appear {dlb}
int mons_rarity(int mcls)
{
    int monster_rarity = 0;
    int (*fnc_rarity) (int) = 0;

    // now, what about pandemonium ??? {dlb}
    if (you.level_type == LEVEL_ABYSS)
        return mons_rare_abyss(mcls);
    else
    {
        fnc_rarity =
             ((you.where_are_you == BRANCH_DIS)           ? mons_dis_rare :
              (you.where_are_you == BRANCH_GEHENNA)       ? mons_gehenna_rare :
              (you.where_are_you == BRANCH_COCYTUS)       ? mons_cocytus_rare :
              (you.where_are_you == BRANCH_TARTARUS)      ? mons_tartarus_rare :
              (you.where_are_you == BRANCH_ORCISH_MINES)  ? mons_mineorc_rare :
              (you.where_are_you == BRANCH_HIVE)          ? mons_hive_rare :
              (you.where_are_you == BRANCH_LAIR)          ? mons_lair_rare :
              (you.where_are_you == BRANCH_SLIME_PITS)    ? mons_pitslime_rare :
              (you.where_are_you == BRANCH_CRYPT)         ? mons_crypt_rare :
              (you.where_are_you == BRANCH_HALL_OF_BLADES)?mons_hallblade_rare :
              (you.where_are_you == BRANCH_HALL_OF_ZOT)   ? mons_hallzot_rare :
              (you.where_are_you == BRANCH_SNAKE_PIT)     ? mons_pitsnake_rare :
              (you.where_are_you == BRANCH_ELVEN_HALLS)   ? mons_hallelf_rare :
              (you.where_are_you == BRANCH_TOMB)          ? mons_tomb_rare :
              (you.where_are_you == BRANCH_SWAMP)         ? mons_swamp_rare :
              (you.where_are_you == BRANCH_VAULTS)        ? mons_standard_rare
                                                          : mons_standard_rare);

        monster_rarity = fnc_rarity(mcls);
    }

    return (monster_rarity);
}                               // end mons_rarity()

bool mons_abyss(int mcls)
{
    switch (mcls)
    {
    case MONS_ABOMINATION_LARGE:
    case MONS_ABOMINATION_SMALL:
    case MONS_AIR_ELEMENTAL:
    case MONS_ANCIENT_LICH:
    case MONS_BALRUG:
    case MONS_BLUE_DEATH:
    case MONS_BLUE_DEVIL:
    case MONS_BRAIN_WORM:
    case MONS_CACODEMON:
    case MONS_CLAY_GOLEM:
    case MONS_CRYSTAL_GOLEM:
    case MONS_DANCING_WEAPON:
    case MONS_DEMONIC_CRAWLER:
    case MONS_EARTH_ELEMENTAL:
    case MONS_EFREET:
    case MONS_EXECUTIONER:
    case MONS_EYE_OF_DEVASTATION:
    case MONS_EYE_OF_DRAINING:
    case MONS_FIRE_ELEMENTAL:
    case MONS_FLAYED_GHOST:
    case MONS_FLYING_SKULL:
    case MONS_FREEZING_WRAITH:
    case MONS_FUNGUS:
    case MONS_GIANT_EYEBALL:
    case MONS_GIANT_ORANGE_BRAIN:
    case MONS_GIANT_SPORE:
    case MONS_GREAT_ORB_OF_EYES:
    case MONS_GREEN_DEATH:
    case MONS_GUARDIAN_NAGA:
    case MONS_HAIRY_DEVIL:
    case MONS_HELLION:
    case MONS_HELLWING:
    case MONS_HELL_HOUND:
    case MONS_HELL_KNIGHT:
    case MONS_HUNGRY_GHOST:
    case MONS_ICE_BEAST:
    case MONS_ICE_DEVIL:
    case MONS_IMP:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_IRON_DEVIL:
    case MONS_IRON_GOLEM:
    case MONS_JELLY:
    case MONS_SKELETON_LARGE:
    case MONS_LEMURE:
    case MONS_LICH:
    case MONS_LOROCYPROCA:
    case MONS_MANES:
    case MONS_MIDGE:
    case MONS_MUMMY:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_NECROMANCER:
    case MONS_NECROPHAGE:
    case MONS_NEQOXEC:
    case MONS_ORANGE_DEMON:
    case MONS_PHANTOM:
    case MONS_PIT_FIEND:
    case MONS_RAKSHASA:
    case MONS_REAPER:
    case MONS_RED_DEVIL:
    case MONS_ROTTING_DEVIL:
    case MONS_SHADOW:
    case MONS_SHADOW_DEMON:
    case MONS_SHADOW_IMP:
    case MONS_SHINING_EYE:
    case MONS_SKELETAL_DRAGON:
    case MONS_SKELETAL_WARRIOR:
    case MONS_SKELETON_SMALL:
    case MONS_SMOKE_DEMON:
    case MONS_SOUL_EATER:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_SPINY_WORM:
    case MONS_STONE_GOLEM:
    case MONS_SUN_DEMON:
    case MONS_TENTACLED_MONSTROSITY:
    case MONS_TOENAIL_GOLEM:
    case MONS_TORMENTOR:
    case MONS_UFETUBUS:
    case MONS_UGLY_THING:
    case MONS_UNSEEN_HORROR:
    case MONS_VAMPIRE:
    case MONS_VAPOUR:
    case MONS_VERY_UGLY_THING:
    case MONS_WHITE_IMP:
    case MONS_WIGHT:
    case MONS_WIZARD:
    case MONS_WOOD_GOLEM:
    case MONS_WRAITH:
    case MONS_YNOXINUL:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
    case MONS_SIMULACRUM_LARGE:
    case MONS_SIMULACRUM_SMALL:
        return true;
    default:
        return false;
    }
}                               // end mons_abyss()

int mons_rare_abyss(int mcls)
{
    switch (mcls)
    {
    case MONS_ABOMINATION_LARGE:
    case MONS_ABOMINATION_SMALL:
        return 99;

    case MONS_LEMURE:
    case MONS_MANES:
    case MONS_MIDGE:
    case MONS_UFETUBUS:
    case MONS_WHITE_IMP:
        return 80;

    case MONS_HELLWING:
    case MONS_NEQOXEC:
    case MONS_ORANGE_DEMON:
    case MONS_SMOKE_DEMON:
    case MONS_YNOXINUL:
        return 50;

    case MONS_SKELETON_LARGE:
    case MONS_SKELETAL_WARRIOR:
    case MONS_SKELETON_SMALL:
        return 40;

    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
        return 35;

    case MONS_SKELETAL_DRAGON:
        return 20;

    case MONS_EFREET:
        return 18;

    case MONS_RAKSHASA:
        return 17;

    case MONS_BRAIN_WORM:
        return 16;

    case MONS_FLYING_SKULL:
    case MONS_FREEZING_WRAITH:
    case MONS_GIANT_ORANGE_BRAIN:
    case MONS_VERY_UGLY_THING:
    case MONS_WRAITH:
        return 15;

    case MONS_EYE_OF_DRAINING:
    case MONS_LICH:
        return 14;

    case MONS_INSUBSTANTIAL_WISP:
    case MONS_UNSEEN_HORROR:
        return 12;

    case MONS_HELL_HOUND:
    case MONS_HUNGRY_GHOST:
    case MONS_SHADOW:
        return 11;

    case MONS_BALRUG:
    case MONS_BLUE_DEATH:
    case MONS_BLUE_DEVIL:
    case MONS_CACODEMON:
    case MONS_DEMONIC_CRAWLER:
    case MONS_EXECUTIONER:
    case MONS_GREEN_DEATH:
    case MONS_GUARDIAN_NAGA:
    case MONS_HAIRY_DEVIL:
    case MONS_HELLION:
    case MONS_ICE_DEVIL:
    case MONS_IMP:
    case MONS_LOROCYPROCA:
    case MONS_MUMMY:
    case MONS_NECROPHAGE:
    case MONS_ROTTING_DEVIL:
    case MONS_SHADOW_DEMON:
    case MONS_SHADOW_IMP:
    case MONS_SUN_DEMON:
    case MONS_WIGHT:
        return 10;

    case MONS_ICE_BEAST:
    case MONS_JELLY:
    case MONS_TORMENTOR:
    case MONS_VAMPIRE:
    case MONS_VAPOUR:
    case MONS_SIMULACRUM_LARGE:
    case MONS_SIMULACRUM_SMALL:
        return 9;

    case MONS_FUNGUS:
    case MONS_GIANT_EYEBALL:
    case MONS_PHANTOM:
    case MONS_REAPER:
        return 8;

    case MONS_SOUL_EATER:
        return 7;

    case MONS_IRON_DEVIL:
        return 6;

    case MONS_ANCIENT_LICH:
    case MONS_CLAY_GOLEM:
    case MONS_GREAT_ORB_OF_EYES:
    case MONS_IRON_GOLEM:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_PIT_FIEND:
    case MONS_RED_DEVIL:
    case MONS_SHINING_EYE:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_SPINY_WORM:
    case MONS_STONE_GOLEM:
    case MONS_TENTACLED_MONSTROSITY:
    case MONS_WIZARD:
    case MONS_WOOD_GOLEM:
        return 5;

    case MONS_AIR_ELEMENTAL:
    case MONS_EARTH_ELEMENTAL:
    case MONS_FIRE_ELEMENTAL:
    case MONS_FLAYED_GHOST:
        return 4;

    case MONS_CRYSTAL_GOLEM:
    case MONS_EYE_OF_DEVASTATION:
    case MONS_HELL_KNIGHT:
    case MONS_NECROMANCER:
    case MONS_UGLY_THING:
        return 3;

    case MONS_DANCING_WEAPON:
    case MONS_GIANT_SPORE:
        return 2;

    case MONS_TOENAIL_GOLEM:
        return 1;

    default:
        return 0;
    }
}                               // end mons_rare_abyss()

bool mons_pan(int mcls)
{
    switch (mcls)
    {
    // icky monsters
    case MONS_BRAIN_WORM:
    case MONS_FUNGUS:
    case MONS_GIANT_ORANGE_BRAIN:
    case MONS_JELLY:
    case MONS_SLIME_CREATURE:
    // undead
    case MONS_FLAYED_GHOST:
    case MONS_FLYING_SKULL:
    case MONS_FREEZING_WRAITH:
    case MONS_HUNGRY_GHOST:
    case MONS_SKELETON_LARGE:
    case MONS_LICH:
    case MONS_MUMMY:
    case MONS_NECROPHAGE:
    case MONS_PHANTOM:
    case MONS_SHADOW:
    case MONS_SKELETON_SMALL:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_VAMPIRE:
    case MONS_WIGHT:
    case MONS_WRAITH:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
    case MONS_SIMULACRUM_LARGE:
    case MONS_SIMULACRUM_SMALL:
    // "things"
    case MONS_ABOMINATION_LARGE:
    case MONS_ABOMINATION_SMALL:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_PULSATING_LUMP:
    case MONS_UNSEEN_HORROR:
    // eyes
    case MONS_EYE_OF_DRAINING:
    case MONS_GIANT_EYEBALL:
    case MONS_GREAT_ORB_OF_EYES:
    // malign beings
    case MONS_EFREET:
    case MONS_RAKSHASA:
    //case MONS_RAKSHASA_FAKE:    //jmf: FIXME: really create these?
                                  // I'm guessing not -- bwr
    // golems
    case MONS_CLAY_GOLEM:
    case MONS_CRYSTAL_GOLEM:
    case MONS_IRON_GOLEM:
    case MONS_STONE_GOLEM:
    case MONS_TOENAIL_GOLEM:
    case MONS_WOOD_GOLEM:
    // dragon(s)
    case MONS_MOTTLED_DRAGON:
    // elementals
    case MONS_AIR_ELEMENTAL:
    case MONS_EARTH_ELEMENTAL:
    case MONS_FIRE_ELEMENTAL:
    // humanoids
    case MONS_NECROMANCER:
    case MONS_STONE_GIANT:
    case MONS_WIZARD:
    // demons
    case MONS_BALRUG:
    case MONS_BLUE_DEATH:
    case MONS_CACODEMON:
    case MONS_EXECUTIONER:
    case MONS_GREEN_DEATH:
    case MONS_HELLWING:
    case MONS_LEMURE:
    case MONS_MANES:
    case MONS_MIDGE:
    case MONS_NEQOXEC:
    case MONS_ORANGE_DEMON:
    case MONS_SMOKE_DEMON:
    case MONS_UFETUBUS:
    case MONS_WHITE_IMP:
    case MONS_YNOXINUL:
        return true;
    default:
        return false;
    }
}                               // end mons_pan()

/* ******************** END EXTERNAL FUNCTIONS ******************** */

static int mons_dis_level(int mcls)
{
    int mlev = 26;

    switch (mcls)
    {
    case MONS_CLAY_GOLEM:
    case MONS_IMP:
    case MONS_NECROPHAGE:
    case MONS_RED_DEVIL:
    case MONS_SKELETAL_WARRIOR:
    case MONS_ZOMBIE_LARGE:
        mlev++;
        break;

    case MONS_HELL_HOUND:
    case MONS_HELL_KNIGHT:
    case MONS_SKELETON_LARGE:
    case MONS_PHANTOM:
    case MONS_ROTTING_DEVIL:
    case MONS_SHADOW:
    case MONS_SKELETON_SMALL:
    case MONS_STONE_GOLEM:
    case MONS_TORMENTOR:
    case MONS_WIGHT:
    case MONS_ZOMBIE_SMALL:
        mlev += 2;
        break;

    case MONS_EFREET:
    case MONS_FLYING_SKULL:
    case MONS_HELLION:
    case MONS_HELL_HOG:
    case MONS_IRON_GOLEM:
    case MONS_MUMMY:
        mlev += 3;
        break;

    case MONS_FLAYED_GHOST:
    case MONS_FREEZING_WRAITH:
    case MONS_HAIRY_DEVIL:
    case MONS_IRON_DEVIL:
    case MONS_VAMPIRE:
    case MONS_WRAITH:
        mlev += 4;
        break;

    case MONS_BLUE_DEVIL:
    case MONS_DANCING_WEAPON:
    case MONS_ICE_DEVIL:
    case MONS_ICE_DRAGON:
    case MONS_LICH:
    case MONS_REAPER:
    case MONS_SOUL_EATER:
    case MONS_SPECTRAL_WARRIOR:
        mlev += 5;
        break;

    case MONS_ANCIENT_LICH:
    case MONS_FIEND:
    case MONS_IRON_DRAGON:
    case MONS_SKELETAL_DRAGON:
        mlev += 6;
        break;

    default:
        return 0;
    }

    return (mlev);
}                               // end mons_dis_level()

static int mons_dis_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_IMP:
    case MONS_IRON_DEVIL:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
        return 99;

    case MONS_REAPER:
        return 77;

    case MONS_TORMENTOR:
        return 66;

    case MONS_RED_DEVIL:
    case MONS_SKELETAL_WARRIOR:
        return 50;

    case MONS_WRAITH:
        return 48;

    case MONS_SHADOW:
        return 56;

    case MONS_HELL_HOUND:
        return 46;

    case MONS_MUMMY:
    case MONS_WIGHT:
        return 45;

    case MONS_HELLION:
    case MONS_BLUE_DEVIL:
        return 40;

    case MONS_FLYING_SKULL:
        return 35;

    case MONS_FREEZING_WRAITH:
    case MONS_ICE_DEVIL:
        return 30;

    case MONS_FLAYED_GHOST:
    case MONS_SKELETON_LARGE:
    case MONS_NECROPHAGE:
    case MONS_SKELETON_SMALL:
        return 25;

    case MONS_HELL_HOG:
    case MONS_SKELETAL_DRAGON:
        return 20;

    case MONS_VAMPIRE:
        return 19;

    case MONS_PHANTOM:
        return 17;

    case MONS_HAIRY_DEVIL:
        return 15;

    case MONS_CLAY_GOLEM:
    case MONS_DANCING_WEAPON:
    case MONS_EFREET:
    case MONS_HELL_KNIGHT:
    case MONS_IRON_GOLEM:
    case MONS_LICH:
    case MONS_ROTTING_DEVIL:
    case MONS_SOUL_EATER:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_STONE_GOLEM:
        return 10;

    case MONS_IRON_DRAGON:
        return 5;

    case MONS_ANCIENT_LICH:
    case MONS_FIEND:
        return 3;

    default:
        return 0;
    }
}                               // end mons_dis_rare()

static int mons_gehenna_level(int mcls)
{
    int mlev = 26;

    switch (mcls)
    {
    case MONS_CLAY_GOLEM:
    case MONS_SKELETON_LARGE:
    case MONS_RED_DEVIL:
    case MONS_SKELETON_SMALL:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
        mlev++;
        break;

    case MONS_HELL_HOG:
    case MONS_HELL_HOUND:
    case MONS_IMP:
    case MONS_NECROPHAGE:
    case MONS_STONE_GOLEM:
        mlev += 2;
        break;

    case MONS_FLYING_SKULL:
    case MONS_IRON_GOLEM:
    case MONS_MUMMY:
    case MONS_PHANTOM:
    case MONS_ROTTING_DEVIL:
    case MONS_SHADOW:
    case MONS_WIGHT:
        mlev += 3;
        break;

    case MONS_HAIRY_DEVIL:
    case MONS_HELL_KNIGHT:
    case MONS_VAMPIRE:
    case MONS_WRAITH:
        mlev += 4;
        break;

    case MONS_EFREET:
    case MONS_FLAYED_GHOST:
    case MONS_HELLION:
    case MONS_TORMENTOR:
        mlev += 5;
        break;

    case MONS_ANCIENT_LICH:
    case MONS_FIEND:
    case MONS_LICH:
    case MONS_PIT_FIEND:
    case MONS_REAPER:
    case MONS_SERPENT_OF_HELL:
    case MONS_SKELETAL_DRAGON:
    case MONS_SOUL_EATER:
    case MONS_SPECTRAL_WARRIOR:
        mlev += 6;
        break;

    default:
        return 0;
    }

    return (mlev);
}                               // end mons_gehenna_level()

static int mons_gehenna_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_SKELETON_LARGE:
    case MONS_SKELETON_SMALL:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
        return 99;

    case MONS_MUMMY:
        return 70;

    case MONS_SHADOW:
        return 61;

    case MONS_RED_DEVIL:
    case MONS_WIGHT:
        return 60;

    case MONS_HELLION:
        return 54;

    case MONS_WRAITH:
        return 53;

    case MONS_NECROPHAGE:
    case MONS_ROTTING_DEVIL:
        return 50;

    case MONS_VAMPIRE:
        return 44;

    case MONS_FLYING_SKULL:
    case MONS_REAPER:
        return 43;

    case MONS_TORMENTOR:
        return 42;

    case MONS_HELL_HOUND:
        return 41;

    case MONS_FLAYED_GHOST:
    case MONS_PHANTOM:
        return 32;

    case MONS_HELL_HOG:
    case MONS_IMP:
    case MONS_IRON_DEVIL:
        return 30;

    case MONS_LICH:
        return 25;

    case MONS_HELL_KNIGHT:
        return 21;

    case MONS_HAIRY_DEVIL:
    case MONS_SPECTRAL_WARRIOR:
        return 20;

    case MONS_CLAY_GOLEM:
    case MONS_SKELETAL_DRAGON:
        return 10;

    case MONS_STONE_GOLEM:
        return 8;

    case MONS_PIT_FIEND:
        return 7;

    case MONS_EFREET:
    case MONS_FIEND:
    case MONS_IRON_GOLEM:
    case MONS_SOUL_EATER:
        return 5;

    case MONS_ANCIENT_LICH:
    case MONS_SERPENT_OF_HELL:
        return 4;

    default:
        return 0;
    }
}                               // end mons_gehenna_rare()

static int mons_cocytus_level(int mcls)
{
    int mlev = 26;

    switch (mcls)
    {
    case MONS_SKELETON_LARGE:
    case MONS_NECROPHAGE:
    case MONS_SKELETAL_WARRIOR:
    case MONS_SKELETON_SMALL:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
    case MONS_SIMULACRUM_LARGE:
    case MONS_SIMULACRUM_SMALL:
        mlev++;
        break;

    case MONS_BLUE_DEVIL:
    case MONS_ICE_BEAST:
    case MONS_PHANTOM:
    case MONS_SHADOW:
        mlev += 2;
        break;

    case MONS_FLYING_SKULL:
    case MONS_ROTTING_DEVIL:
    case MONS_VAMPIRE:
    case MONS_WIGHT:
        mlev += 3;
        break;

    case MONS_FREEZING_WRAITH:
    case MONS_HAIRY_DEVIL:
    case MONS_HUNGRY_GHOST:
    case MONS_MUMMY:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_WRAITH:
        mlev += 4;
        break;

    case MONS_ICE_DEVIL:
    case MONS_ICE_DRAGON:
    case MONS_TORMENTOR:
        mlev += 5;
        break;

    case MONS_ANCIENT_LICH:
    case MONS_LICH:
    case MONS_REAPER:
    case MONS_SKELETAL_DRAGON:
    case MONS_SOUL_EATER:
        mlev += 6;
        break;

    default:
        return 0;
    }

    return (mlev);
}                               // end mons_cocytus_level()

static int mons_cocytus_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_FREEZING_WRAITH:
        return 87;

    case MONS_ICE_BEAST:
    case MONS_SKELETON_LARGE:
    case MONS_SKELETON_SMALL:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
        return 85;

    case MONS_BLUE_DEVIL:
    case MONS_ICE_DEVIL:
        return 76;

    case MONS_FLYING_SKULL:
        return 57;

    case MONS_SHADOW:
        return 56;

    case MONS_SKELETAL_WARRIOR:
        return 50;

    case MONS_REAPER:
        return 47;

    case MONS_WIGHT:
    case MONS_WRAITH:
        return 45;

    case MONS_ICE_DRAGON:
        return 38;

    case MONS_ROTTING_DEVIL:
    case MONS_TORMENTOR:
        return 37;

    case MONS_MUMMY:
        return 35;

    case MONS_VAMPIRE:
        return 34;

    case MONS_HAIRY_DEVIL:
    case MONS_HUNGRY_GHOST:
        return 26;

    case MONS_NECROPHAGE:
    case MONS_PHANTOM:
        return 25;

    case MONS_SPECTRAL_WARRIOR:
        return 20;

    case MONS_SOUL_EATER:
        return 19;

    case MONS_LICH:
    case MONS_SKELETAL_DRAGON:
    case MONS_SIMULACRUM_LARGE:
    case MONS_SIMULACRUM_SMALL:
        return 12;

    case MONS_ANCIENT_LICH:
        return 5;

    default:
        return 0;
    }
}                               // end mons_cocytus_rare()

static int mons_tartarus_level(int mcls)
{
    int mlev = 26;

    switch (mcls)
    {
    case MONS_IMP:
    case MONS_SKELETON_LARGE:
    case MONS_RED_DEVIL:
    case MONS_SHADOW_IMP:
    case MONS_SKELETAL_WARRIOR:
    case MONS_SKELETON_SMALL:
        mlev++;
        break;

    case MONS_HELL_KNIGHT:
    case MONS_NECROPHAGE:
    case MONS_PHANTOM:
    case MONS_WIGHT:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
        mlev += 2;
        break;

    case MONS_FREEZING_WRAITH:
    case MONS_HELL_HOUND:
    case MONS_NECROMANCER:
    case MONS_SHADOW:
    case MONS_SHADOW_DEMON:
    case MONS_WRAITH:
        mlev += 3;
        break;

    case MONS_BLUE_DEVIL:
    case MONS_FLAYED_GHOST:
    case MONS_HUNGRY_GHOST:
    case MONS_ICE_DEVIL:
    case MONS_MUMMY:
    case MONS_SKELETAL_DRAGON:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_TORMENTOR:
    case MONS_SIMULACRUM_LARGE:
    case MONS_SIMULACRUM_SMALL:
        mlev += 4;
        break;

    case MONS_FLYING_SKULL:
    case MONS_HELLION:
    case MONS_REAPER:
    case MONS_ROTTING_DEVIL:
    case MONS_SHADOW_DRAGON:
    case MONS_VAMPIRE:
        mlev += 5;
        break;

    case MONS_ANCIENT_LICH:
    case MONS_HAIRY_DEVIL:
    case MONS_LICH:
    case MONS_SOUL_EATER:
        mlev += 6;
        break;

    default:
        return 0;
    }

    return (mlev);
}                               // end mons_tartarus_level()

static int mons_tartarus_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_SKELETON_LARGE:
    case MONS_SHADOW_IMP:
    case MONS_SKELETAL_WARRIOR:
    case MONS_SKELETON_SMALL:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
        return 99;

    case MONS_SHADOW:
        return 92;

    case MONS_REAPER:
        return 73;

    case MONS_NECROPHAGE:
        return 72;

    case MONS_WIGHT:
        return 71;

    case MONS_ROTTING_DEVIL:
        return 62;

    case MONS_FREEZING_WRAITH:
        return 60;

    case MONS_FLYING_SKULL:
        return 53;

    case MONS_HELL_HOUND:
    case MONS_PHANTOM:
    case MONS_WRAITH:
        return 52;

    case MONS_SHADOW_DEMON:
        return 50;

    case MONS_SPECTRAL_WARRIOR:
        return 45;

    case MONS_VAMPIRE:
        return 44;

    case MONS_HELLION:
    case MONS_TORMENTOR:
        return 42;

    case MONS_SKELETAL_DRAGON:
        return 40;

    case MONS_SOUL_EATER:
        return 35;

    case MONS_ICE_DEVIL:        // not really appropriate for a fiery hell
        return 34;

    case MONS_MUMMY:
        return 33;

    case MONS_BLUE_DEVIL:
    case MONS_HUNGRY_GHOST:
        return 32;

    case MONS_FLAYED_GHOST:
    case MONS_HAIRY_DEVIL:
        return 30;

    case MONS_LICH:
        return 24;

    case MONS_IMP:
    case MONS_SHADOW_DRAGON:
        return 20;

    case MONS_RED_DEVIL:
        return 13;

    case MONS_HELL_KNIGHT:
        return 14;

    case MONS_NECROMANCER:
    case MONS_SIMULACRUM_LARGE:
    case MONS_SIMULACRUM_SMALL:
        return 12;

    case MONS_ANCIENT_LICH:
        return 6;

    default:
        return 0;
    }
}

static int mons_mineorc_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_ORCISH_MINES] + 1;

    switch (mcls)
    {
    case MONS_HOBGOBLIN:
    case MONS_ORC_PRIEST:
    case MONS_ORC_WARRIOR:
        mlev++;
        break;

    case MONS_GNOLL:
    case MONS_OGRE:
    case MONS_WARG:
    case MONS_ORC_KNIGHT:
    case MONS_ORC_WIZARD:
        mlev += 2;
        break;

    case MONS_CYCLOPS:
    case MONS_IRON_TROLL:
    case MONS_OGRE_MAGE:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_ORC_SORCERER:
    case MONS_ORC_WARLORD:
    case MONS_ROCK_TROLL:
    case MONS_STONE_GIANT:
    case MONS_TROLL:
    case MONS_TWO_HEADED_OGRE:
    case MONS_ETTIN:
        mlev += 3;
        break;

    case MONS_FUNGUS:
    case MONS_GOBLIN:
    case MONS_ORC:
    default:
        mlev += 0;
    }

    return (mlev);
}                               // end mons_mineorc_level()

static int mons_mineorc_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_ORC:
        return 300;

    case MONS_GOBLIN:
    case MONS_ORC_WARRIOR:
        return 30;

    case MONS_HOBGOBLIN:
    case MONS_OGRE:
        return 20;

    case MONS_TROLL:
    case MONS_WARG:
        return 13;

    case MONS_FUNGUS:
    case MONS_ORC_KNIGHT:
    case MONS_ORC_PRIEST:
    case MONS_ORC_SORCERER:
    case MONS_ORC_WIZARD:
        return 10;

    case MONS_ORC_WARLORD:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_CYCLOPS:
    case MONS_TWO_HEADED_OGRE:
        return 5;

    case MONS_ETTIN:
    case MONS_IRON_TROLL:
    case MONS_ROCK_TROLL:
    case MONS_STONE_GIANT:
        return 3;

    case MONS_GNOLL:
        return 2;

    case MONS_OGRE_MAGE:
        return 1;

    default:
        return 0;
    }
}                               // end mons_mineorc_rare()

static int mons_hive_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_HIVE] + 1;

    switch (mcls)
    {
    case MONS_PLANT:
    case MONS_KILLER_BEE:
        mlev += 0;
        break;

    case MONS_KILLER_BEE_LARVA:
        mlev += 2;
        break;

    default:
        return 99;
    }

    return (mlev);
}                               // end mons_hive_level()

static int mons_hive_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_KILLER_BEE:
        return 300;

    case MONS_PLANT:
        return 100;

    case MONS_KILLER_BEE_LARVA:
        return 50;

    default:
        return 0;
    }
}                               // end mons_hive_rare()

static int mons_lair_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_LAIR] + 1;

    switch (mcls)
    {
    case MONS_GIANT_GECKO:
    case MONS_GIANT_BAT:
    case MONS_JACKAL:
    case MONS_GIANT_NEWT:
    case MONS_RAT:
    case MONS_QUOKKA:
        mlev += 0;
        break;

    case MONS_GIANT_CENTIPEDE:
    case MONS_GIANT_IGUANA:
        mlev++;
        break;

    case MONS_GIANT_FROG:
    case MONS_GILA_MONSTER:
    case MONS_GREY_RAT:
    case MONS_HOUND:
    case MONS_BLACK_BEAR:
        mlev += 2;
        break;

    case MONS_WORM:
    case MONS_WOLF:
        mlev += 3;
        break;

    case MONS_FUNGUS:
    case MONS_GIANT_BROWN_FROG:
    case MONS_GIANT_LIZARD:
    case MONS_GIANT_MITE:
    case MONS_GREEN_RAT:
    case MONS_SCORPION:
    case MONS_SNAKE:
        mlev += 4;
        break;

    case MONS_BROWN_SNAKE:
    case MONS_BUTTERFLY:
    case MONS_GIANT_BEETLE:
    case MONS_GIANT_SLUG:
    case MONS_HIPPOGRIFF:
    case MONS_PLANT:
    case MONS_SPINY_FROG:
    case MONS_WAR_DOG:
    case MONS_YELLOW_WASP:
    case MONS_BEAR:
        mlev += 5;
        break;

    case MONS_BLINK_FROG:
    case MONS_GIANT_SNAIL:
    case MONS_GIANT_SPORE:
    case MONS_KOMODO_DRAGON:
    case MONS_ORANGE_RAT:
    case MONS_SHEEP:
    case MONS_STEAM_DRAGON:
    case MONS_WOLF_SPIDER:
    case MONS_YAK:
    case MONS_GRIZZLY_BEAR:
        mlev += 6;
        break;

    case MONS_BLACK_SNAKE:
    case MONS_BRAIN_WORM:
    case MONS_BUMBLEBEE:
    case MONS_FIREDRAKE:
    case MONS_HYDRA:
    case MONS_OKLOB_PLANT:
    case MONS_WYVERN:
        mlev += 7;
        break;

    case MONS_ELEPHANT_SLUG:
    case MONS_POLAR_BEAR:
    case MONS_GRIFFON:
    case MONS_LINDWURM:
    case MONS_REDBACK:
    case MONS_WANDERING_MUSHROOM:
        mlev += 8;
        break;

    case MONS_BORING_BEETLE:
    case MONS_BOULDER_BEETLE:
    case MONS_DEATH_YAK:
    case MONS_SPINY_WORM:
        mlev += 9;
        break;

    default:
        return 99;
    }

    return (mlev);
}                               // end mons_lair_level()

static int mons_lair_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_RAT:
        return 200;

    case MONS_GIANT_BAT:
    case MONS_GIANT_BROWN_FROG:
    case MONS_GIANT_FROG:
    case MONS_GREY_RAT:
    case MONS_QUOKKA:
        return 99;

    case MONS_BROWN_SNAKE:
    case MONS_GIANT_LIZARD:
        return 90;

    case MONS_PLANT:
    case MONS_SNAKE:
        return 80;

    case MONS_SPINY_FROG:
        return 75;

    case MONS_JACKAL:
    case MONS_GIANT_IGUANA:
    case MONS_GILA_MONSTER:
        return 70;

    case MONS_GREEN_RAT:
        return 64;

    case MONS_HOUND:
        return 60;

    case MONS_GIANT_SNAIL:
        return 56;

    case MONS_GIANT_SLUG:
        return 55;

    case MONS_FUNGUS:
    case MONS_GIANT_GECKO:
    case MONS_GIANT_CENTIPEDE:
    case MONS_HIPPOGRIFF:
    case MONS_HYDRA:
    case MONS_KOMODO_DRAGON:
    case MONS_YAK:
        return 50;

    case MONS_BLACK_SNAKE:
        return 47;

    case MONS_BLINK_FROG:
        return 45;

    case MONS_SHEEP:
    case MONS_FIREDRAKE:
        return 36;

    case MONS_WAR_DOG:
        return 35;

    case MONS_WORM:
    case MONS_GIANT_MITE:
    case MONS_GRIFFON:
    case MONS_DEATH_YAK:
    case MONS_ELEPHANT_SLUG:
        return 30;

    case MONS_BORING_BEETLE:
        return 29;

    case MONS_BOULDER_BEETLE:
    case MONS_GIANT_NEWT:
    case MONS_WOLF:
    case MONS_WYVERN:
        return 20;

    case MONS_BLACK_BEAR:
    case MONS_BEAR:
    case MONS_GRIZZLY_BEAR:
    case MONS_POLAR_BEAR:
        return 15;

    case MONS_GIANT_BEETLE:
    case MONS_SCORPION:
    case MONS_OKLOB_PLANT:
    case MONS_STEAM_DRAGON:
    case MONS_LINDWURM:
    case MONS_ORANGE_RAT:
        return 10;

    case MONS_SPINY_WORM:
        return 9;

    case MONS_WANDERING_MUSHROOM:
    case MONS_REDBACK:
        return 8;

    case MONS_BRAIN_WORM:
    case MONS_BUMBLEBEE:
        return 7;

    case MONS_WOLF_SPIDER:
        return 6;

    case MONS_YELLOW_WASP:
    case MONS_BUTTERFLY:
        return 5;

    case MONS_GIANT_SPORE:
        return 2;

    default:
        return 0;
    }
}                               // end mons_lair_rare()

static int mons_pitslime_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_SLIME_PITS] + 1;


    switch (mcls)
    {
    case MONS_JELLY:
    case MONS_OOZE:
    case MONS_ACID_BLOB:
    case MONS_GIANT_SPORE:
    case MONS_GIANT_EYEBALL:
        mlev++;
        break;

    case MONS_BROWN_OOZE:
    case MONS_SLIME_CREATURE:
    case MONS_EYE_OF_DRAINING:
        mlev += 2;
        break;

    case MONS_GIANT_AMOEBA:
    case MONS_AZURE_JELLY:
    case MONS_SHINING_EYE:
        mlev += 3;
        break;

    case MONS_PULSATING_LUMP:
    case MONS_GREAT_ORB_OF_EYES:
    case MONS_EYE_OF_DEVASTATION:
        mlev += 4;
        break;

    case MONS_DEATH_OOZE:
    case MONS_TENTACLED_MONSTROSITY:
    case MONS_GIANT_ORANGE_BRAIN:
        mlev += 5;
        break;

    default:
        mlev += 0;
        break;
    }

    return (mlev);
}                               // end mons_pitslime_level()

static int mons_pitslime_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_JELLY:
        return 300;

    case MONS_SLIME_CREATURE:
        return 200;

    case MONS_BROWN_OOZE:
        return 150;

    case MONS_GIANT_AMOEBA:
    case MONS_ACID_BLOB:
        return 100;

    case MONS_OOZE:
    case MONS_AZURE_JELLY:
    case MONS_GIANT_SPORE:
    case MONS_GIANT_EYEBALL:
    case MONS_EYE_OF_DRAINING:
    case MONS_SHINING_EYE:
        return 50;

    case MONS_DEATH_OOZE:
    case MONS_GREAT_ORB_OF_EYES:
    case MONS_EYE_OF_DEVASTATION:
        return 30;

    case MONS_PULSATING_LUMP:
    case MONS_GIANT_ORANGE_BRAIN:
        return 20;

    case MONS_TENTACLED_MONSTROSITY:
        return 2;

    default:
        return 0;
    }
}                               // end mons_pitslime_rare()

static int mons_crypt_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_CRYPT] + 1;

    switch (mcls)
    {
    case MONS_ZOMBIE_SMALL:
        mlev += 0;
        break;

    case MONS_PHANTOM:
    case MONS_SKELETON_SMALL:
    case MONS_SKELETON_LARGE:
    case MONS_ZOMBIE_LARGE:
    case MONS_WIGHT:
        mlev++;
        break;

    case MONS_SHADOW:
    case MONS_HUNGRY_GHOST:
    case MONS_NECROPHAGE:
    case MONS_SKELETAL_WARRIOR:
    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
        mlev += 2;
        break;

    case MONS_NECROMANCER:
    case MONS_PULSATING_LUMP:
    case MONS_FLAYED_GHOST:
    case MONS_GHOUL:
    case MONS_ROTTING_HULK:
    case MONS_WRAITH:
    case MONS_FLYING_SKULL:
        mlev += 3;
        break;

    case MONS_SPECTRAL_WARRIOR:
    case MONS_SHADOW_WRAITH:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAMPIRE_MAGE:
    case MONS_SKELETAL_DRAGON:
    case MONS_ABOMINATION_SMALL:
    case MONS_MUMMY:
    case MONS_VAMPIRE:
    case MONS_ABOMINATION_LARGE:
        mlev += 4;
        break;

    case MONS_REAPER:
    case MONS_ANCIENT_LICH:
    case MONS_LICH:
        mlev += 5;
        break;

    default:
        mlev += 99;
    }

    return (mlev);
}                               // end mons_crypt_level()

static int mons_crypt_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_ZOMBIE_SMALL:
    case MONS_SKELETON_SMALL:
    case MONS_SKELETON_LARGE:
        return 410;

    case MONS_ZOMBIE_LARGE:
        return 300;

    case MONS_SKELETAL_WARRIOR:
        return 75;

    case MONS_NECROPHAGE:
        return 50;

    case MONS_WIGHT:
        return 35;

    case MONS_WRAITH:
        return 33;

    case MONS_SHADOW:
        return 30;

    case MONS_NECROMANCER:
    case MONS_GHOUL:
        return 25;

    case MONS_MUMMY:
    case MONS_SKELETAL_DRAGON:
        return 24;

    case MONS_VAMPIRE:
    case MONS_PHANTOM:
        return 21;

    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAMPIRE_MAGE:
        return 20;

    case MONS_ROTTING_HULK:
        return 17;

    case MONS_SPECTRAL_WARRIOR:
        return 14;

    case MONS_FLYING_SKULL:
    case MONS_FLAYED_GHOST:
        return 13;

    case MONS_HUNGRY_GHOST:
        return 12;

    case MONS_SHADOW_WRAITH:
    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
        return 10;

    case MONS_ANCIENT_LICH:
        return 8;

    case MONS_ABOMINATION_SMALL:
    case MONS_LICH:
    case MONS_REAPER:
        return 5;

    case MONS_ABOMINATION_LARGE:
        return 4;

    case MONS_PULSATING_LUMP:
        return 3;

    default:
        return 0;
    }
}                               // end mons_crypt_rare()

static int mons_pitsnake_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_SNAKE_PIT] + 1;

    switch (mcls)
    {
    case MONS_SMALL_SNAKE:
    case MONS_SNAKE:
        mlev++;
        break;

    case MONS_BROWN_SNAKE:
    case MONS_BLACK_SNAKE:
    case MONS_YELLOW_SNAKE:
    case MONS_GREY_SNAKE:
    case MONS_NAGA:
        mlev += 2;
        break;

    case MONS_NAGA_WARRIOR:
    case MONS_NAGA_MAGE:
        mlev += 3;
        break;

    case MONS_GUARDIAN_NAGA:
        mlev += 4;
        break;

    case MONS_GREATER_NAGA:
        mlev += 5;
        break;

    default:
        mlev += 99;
    }

    return (mlev);
}                               // end mons_pitsnake_level()

static int mons_pitsnake_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_SNAKE:
    case MONS_BROWN_SNAKE:
        return 99;

    case MONS_BLACK_SNAKE:
        return 72;

    case MONS_NAGA:
        return 53;

    case MONS_NAGA_WARRIOR:
    case MONS_NAGA_MAGE:
        return 34;

    case MONS_YELLOW_SNAKE:
    case MONS_GREY_SNAKE:
        return 32;

    case MONS_GREATER_NAGA:
    case MONS_GUARDIAN_NAGA:
    case MONS_SMALL_SNAKE:
        return 15;

    default:
        return 0;
    }
}                               // end mons_pitsnake_rare()

static int mons_hallelf_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_ELVEN_HALLS] + 1;

    switch (mcls)
    {
    case MONS_DEEP_ELF_SOLDIER:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_ORC:
    case MONS_ORC_WARRIOR:
        mlev++;
        break;

    case MONS_ORC_WIZARD:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_SUMMONER:
        mlev += 2;
        break;

    case MONS_FUNGUS:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_SHAPESHIFTER:
    case MONS_ORC_KNIGHT:
        mlev += 3;
        break;

    case MONS_ORC_SORCERER:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_GLOWING_SHAPESHIFTER:
    case MONS_DEEP_ELF_KNIGHT:
        mlev += 4;
        break;

    case MONS_ORC_PRIEST:
    case MONS_ORC_HIGH_PRIEST:
        mlev += 5;
        break;

    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_DEATH_MAGE:
        mlev += 7;
        break;

    default:
        mlev += 99;
        break;
    }

    return (mlev);
}                               // end mons_hallelf_level()

static int mons_hallelf_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_FUNGUS:
        return 300;

    case MONS_DEEP_ELF_SOLDIER:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_MAGE:
        return 100;

    case MONS_DEEP_ELF_KNIGHT:
        return 80;

    case MONS_DEEP_ELF_SUMMONER:
        return 72;

    case MONS_DEEP_ELF_CONJURER:
        return 63;

    case MONS_DEEP_ELF_PRIEST:
        return 44;

    case MONS_SHAPESHIFTER:
        return 25;

    case MONS_ORC:
        return 20;

    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_SORCERER:
        return 17;

    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_ORC_WIZARD:
        return 13;

    case MONS_ORC_WARRIOR:
        return 11;

    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_ORC_SORCERER:
    case MONS_GLOWING_SHAPESHIFTER:
        return 10;

    case MONS_ORC_KNIGHT:
    case MONS_ORC_PRIEST:
    case MONS_ORC_HIGH_PRIEST:
        return 5;

    default:
        return 0;
    }
}                               // end mons_hallelf_rare()

static int mons_tomb_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_CRYPT] + 1;

    switch (mcls)
    {
    case MONS_ZOMBIE_SMALL:
        mlev += 0;
        break;

    case MONS_MUMMY:
    case MONS_ZOMBIE_LARGE:
    case MONS_SKELETON_SMALL:
    case MONS_SKELETON_LARGE:
        mlev++;
        break;

    case MONS_GUARDIAN_MUMMY:
    case MONS_FLYING_SKULL:
    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
        mlev += 2;
        break;

    case MONS_LICH:
    case MONS_ANCIENT_LICH:
    case MONS_MUMMY_PRIEST:
        mlev += 3;
        break;

    default:
        mlev += 99;
    }

    return (mlev);
}                               // end mons_tomb_level()

static int mons_tomb_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_MUMMY:
        return 300;

    case MONS_GUARDIAN_MUMMY:
        return 202;

    case MONS_MUMMY_PRIEST:
        return 40;

    case MONS_FLYING_SKULL:
        return 33;

    case MONS_ZOMBIE_LARGE:
    case MONS_SKELETON_SMALL:
    case MONS_SKELETON_LARGE:
        return 21;

    case MONS_ZOMBIE_SMALL:
        return 20;

    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
        return 10;

    case MONS_LICH:
        return 4;

    case MONS_ANCIENT_LICH:
        return 2;

    default:
        return 0;
    }
}                               // end mons_tomb_rare()

static int mons_swamp_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_SWAMP] + 1;

    switch (mcls)
    {
    case MONS_GIANT_BAT:
    case MONS_GIANT_BLOWFLY:
    case MONS_GIANT_FROG:
    case MONS_GIANT_AMOEBA:
    case MONS_GIANT_SLUG:
    case MONS_GIANT_NEWT:
    case MONS_GIANT_GECKO:
    case MONS_RAT:
    case MONS_SWAMP_DRAKE:
    case MONS_WORM:
        mlev++;
        break;

    case MONS_GIANT_BROWN_FROG:
    case MONS_FUNGUS:
    case MONS_NECROPHAGE:
    case MONS_PLANT:
    case MONS_SNAKE:
    case MONS_BUTTERFLY:
    case MONS_GIANT_LIZARD:
    case MONS_GIANT_MOSQUITO:
    case MONS_GIANT_SNAIL:
    case MONS_HYDRA:
        mlev += 2;
        break;

    case MONS_BROWN_SNAKE:
    case MONS_HUNGRY_GHOST:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_JELLY:
    case MONS_KOMODO_DRAGON:
    case MONS_PHANTOM:
    case MONS_RED_WASP:
    case MONS_SPINY_FROG:
    case MONS_SWAMP_DRAGON:
    case MONS_UGLY_THING:
        mlev += 3;
        break;

    case MONS_BLINK_FROG:
    case MONS_SLIME_CREATURE:
    case MONS_VERY_UGLY_THING:
    case MONS_VAPOUR:
    case MONS_TENTACLED_MONSTROSITY:
        mlev += 4;
        break;

    default:
        mlev += 99;
    }

    return (mlev);
}                               // end mons_swamp_level()

static int mons_swamp_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_GIANT_MOSQUITO:
        return 250;

    case MONS_PLANT:
        return 200;

    case MONS_GIANT_FROG:
        return 150;

    case MONS_GIANT_BLOWFLY:
        return 100;

    case MONS_GIANT_BAT:
    case MONS_FUNGUS:
        return 99;

    case MONS_GIANT_BROWN_FROG:
        return 90;

    case MONS_SWAMP_DRAKE:
        return 80;

    case MONS_HYDRA:
        return 70;

    case MONS_RAT:
        return 61;

    case MONS_SLIME_CREATURE:
        return 54;

    case MONS_SNAKE:
        return 52;

    case MONS_INSUBSTANTIAL_WISP:
        return 43;

    case MONS_BROWN_SNAKE:
        return 33;

    case MONS_RED_WASP:
    case MONS_SWAMP_DRAGON:
    case MONS_SPINY_FROG:
        return 30;

    case MONS_JELLY:
    case MONS_BUTTERFLY:
    case MONS_GIANT_LIZARD:
        return 25;

    case MONS_WORM:
        return 20;

    case MONS_KOMODO_DRAGON:
    case MONS_VERY_UGLY_THING:
    case MONS_VAPOUR:
        return 15;

    case MONS_PHANTOM:
    case MONS_UGLY_THING:
    case MONS_HUNGRY_GHOST:
        return 13;

    case MONS_NECROPHAGE:
        return 12;

    case MONS_BLINK_FROG:
    case MONS_GIANT_AMOEBA:
    case MONS_GIANT_GECKO:
    case MONS_GIANT_NEWT:
    case MONS_GIANT_SLUG:
    case MONS_GIANT_SNAIL:
        return 10;

    case MONS_TENTACLED_MONSTROSITY:
        return 5;

    default:
        return 0;
    }
}                               // end mons_swamp_rare()

static int mons_hallblade_level(int mcls)
{
    if (mcls == MONS_DANCING_WEAPON)
        return (you.branch_stairs[STAIRS_HALL_OF_BLADES] + 1);
    else
        return 0;
}                               // end mons_hallblade_level

static int mons_hallblade_rare(int mcls)
{
    return ((mcls == MONS_DANCING_WEAPON) ? 1000 : 0);
}                               // end mons_hallblade_rare()

//    New branch must be added in:
//    - new_game stair location
//    - down/up stairs (to and back) misc.cc
//    - new_level (2 places) (misc.cc)
//    - item_check items.cc
//    - look_around (direct.cc)
//    - ouch ouch.cc (death message)
//    - and here...

static int mons_hallzot_level(int mcls)
{
    int mlev = you.branch_stairs[STAIRS_HALL_OF_ZOT];

    switch (mcls)
    {
    case MONS_GOLDEN_DRAGON:
    case MONS_GUARDIAN_MUMMY:
        mlev += 6;
        break;
    case MONS_KILLER_KLOWN:
    case MONS_SHADOW_DRAGON:
    case MONS_SKELETAL_DRAGON:
    case MONS_STORM_DRAGON:
        mlev += 5;
        break;
    case MONS_DEATH_COB:
    case MONS_DRAGON:
    case MONS_ICE_DRAGON:
        mlev += 4;
        break;
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_TENTACLED_MONSTROSITY:
        mlev += 3;
        break;
    case MONS_MOTH_OF_WRATH:
        mlev += 2;
        break;
    case MONS_ORB_OF_FIRE:
    case MONS_ELECTRIC_GOLEM:
        mlev += 1;
        break;
    default:
        mlev += 99;             // I think this won't be a problem {dlb}
        break;
    }

    return (mlev);
}                               // end mons_hallzot_level()

static int mons_hallzot_rare(int mcls)
{
    switch (mcls)
    {
    case MONS_MOTH_OF_WRATH:
        return 88;
    case MONS_STORM_DRAGON:
    case MONS_TENTACLED_MONSTROSITY:
        return 50;
    case MONS_GOLDEN_DRAGON:
        return 42;
    case MONS_DEATH_COB:
    case MONS_DRAGON:
    case MONS_ICE_DRAGON:
    case MONS_SKELETAL_DRAGON:
        return 40;
    case MONS_SHADOW_DRAGON:
        return 30;
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_SORCERER:
        return 29;
    case MONS_GUARDIAN_MUMMY:
    case MONS_ELECTRIC_GOLEM:
        return 20;
    case MONS_KILLER_KLOWN:
    case MONS_ORB_OF_FIRE:
        return 15;
    default:
        return 0;
    }
}                               // end mons_hallzot_rare()

static int mons_standard_level(int mcls)
{
    switch (mcls)
    {
    case MONS_GOBLIN:
    case MONS_GIANT_NEWT:
        return 1;

    case MONS_GIANT_COCKROACH:
    case MONS_OOZE:
    case MONS_SMALL_SNAKE:
        return 2;

    case MONS_GIANT_BAT:
    case MONS_KOBOLD:
    case MONS_RAT:
        return 4;

    case MONS_GIANT_GECKO:
    case MONS_GIANT_MITE:
    case MONS_GNOLL:
    case MONS_HOBGOBLIN:
    case MONS_JACKAL:
    case MONS_KILLER_BEE_LARVA:
        return 5;

    case MONS_WORM:
    case MONS_SNAKE:
    case MONS_QUOKKA:
        return 6;

    case MONS_ORC:
        return 7;

    case MONS_FUNGUS:
    case MONS_GIANT_ANT:
    case MONS_GIANT_EYEBALL:
    case MONS_HOUND:
    case MONS_GIANT_IGUANA:
    case MONS_OGRE:
    case MONS_ORC_WIZARD:
    case MONS_PHANTOM:
    case MONS_SCORPION:
        return 8;

    case MONS_BROWN_SNAKE:
    case MONS_CENTAUR:
    case MONS_ICE_BEAST:
    case MONS_IMP:
    case MONS_JELLY:
    case MONS_NECROPHAGE:
    case MONS_QUASIT:
    case MONS_ZOMBIE_SMALL:
        return 9;

    case MONS_DEEP_ELF_SOLDIER:
    case MONS_GIANT_BEETLE:
    case MONS_GIANT_FROG:
    case MONS_GIANT_SPORE:
    case MONS_MUMMY:
    case MONS_ORC_WARRIOR:
    case MONS_STEAM_DRAGON:
    case MONS_WIGHT:
        return 10;

    case MONS_GIANT_LIZARD:
    case MONS_HIPPOGRIFF:
    case MONS_HUNGRY_GHOST:
    case MONS_KILLER_BEE:
    case MONS_SHADOW:
    case MONS_YELLOW_WASP:
        return 11;

    case MONS_EYE_OF_DRAINING:
    case MONS_GILA_MONSTER:
    case MONS_MANTICORE:
    case MONS_PLANT:
    case MONS_UNSEEN_HORROR:
    case MONS_WYVERN:
        return 12;

    case MONS_BIG_KOBOLD:
    case MONS_GIANT_BROWN_FROG:
    case MONS_GIANT_CENTIPEDE:
    case MONS_OKLOB_PLANT:
    case MONS_TROLL:
    case MONS_TWO_HEADED_OGRE:
    case MONS_WOOD_GOLEM:
    case MONS_YAK:
        return 13;

    case MONS_HILL_GIANT:
    case MONS_KOMODO_DRAGON:
    case MONS_SOLDIER_ANT:
    case MONS_WOLF_SPIDER:
    case MONS_WRAITH:
        return 14;

    case MONS_ARMOUR_MIMIC:
    case MONS_BRAIN_WORM:
    case MONS_CYCLOPS:
    case MONS_EFREET:
    case MONS_ETTIN:
    case MONS_EYE_OF_DEVASTATION:
    case MONS_GOLD_MIMIC:
    case MONS_HYDRA:
    case MONS_MOTTLED_DRAGON:
    case MONS_ORC_PRIEST:
    case MONS_POTION_MIMIC:
    case MONS_SCROLL_MIMIC:
    case MONS_SKELETAL_WARRIOR:
    case MONS_WEAPON_MIMIC:
        return 15;

    case MONS_BLINK_FROG:
    case MONS_BUTTERFLY:
    case MONS_GIANT_BLOWFLY:
    case MONS_GUARDIAN_NAGA:
    case MONS_RAKSHASA:
    case MONS_SLIME_CREATURE:
    case MONS_STONE_GOLEM:
    case MONS_VAMPIRE:
    case MONS_WANDERING_MUSHROOM:
    case MONS_ZOMBIE_LARGE:
        return 16;

    case MONS_BOGGART:
    case MONS_CENTAUR_WARRIOR:
    case MONS_CLAY_GOLEM:
    case MONS_GRIFFON:
    case MONS_SHAPESHIFTER:
    case MONS_UGLY_THING:
    case MONS_WIZARD:
    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
        return 17;

    case MONS_DRAGON:
    case MONS_GARGOYLE:
    case MONS_GIANT_AMOEBA:
    case MONS_KOBOLD_DEMONOLOGIST:
        return 18;

    case MONS_GIANT_SLUG:
    case MONS_IRON_GOLEM:
    case MONS_OGRE_MAGE:
    case MONS_ROCK_TROLL:
    case MONS_TOENAIL_GOLEM:
    case MONS_YAKTAUR:
        return 19;

    case MONS_AIR_ELEMENTAL:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_SUMMONER:
    case MONS_EARTH_ELEMENTAL:
    case MONS_FIRE_ELEMENTAL:
    case MONS_GIANT_ORANGE_BRAIN:
    case MONS_GIANT_SNAIL:
    case MONS_GREAT_ORB_OF_EYES:
    case MONS_ICE_DRAGON:
    case MONS_SKELETON_LARGE:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_NECROMANCER:
    case MONS_ORC_KNIGHT:
    case MONS_QUEEN_BEE:
    case MONS_RED_WASP:
    case MONS_SHADOW_WRAITH:
    case MONS_SKELETON_SMALL:
    case MONS_SPINY_WORM:
    case MONS_VERY_UGLY_THING:
        return 20;

    case MONS_BOULDER_BEETLE:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_PULSATING_LUMP:
        return 21;

    case MONS_BORING_BEETLE:
    case MONS_CRYSTAL_GOLEM:
    case MONS_FLAYED_GHOST:
    case MONS_FREEZING_WRAITH:
    case MONS_REDBACK:
    case MONS_SPHINX:
    case MONS_VAPOUR:
        return 22;

    case MONS_ORC_SORCERER:
    case MONS_SHINING_EYE:
        return 23;

    case MONS_BUMBLEBEE:
    case MONS_ORC_WARLORD:
    case MONS_IRON_TROLL:
    case MONS_YAKTAUR_CAPTAIN:
        return 24;

    case MONS_DANCING_WEAPON:
    case MONS_DEEP_TROLL:
    case MONS_FIRE_GIANT:
    case MONS_FROST_GIANT:
    case MONS_HELL_KNIGHT:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_LICH:
    case MONS_STONE_GIANT:
        return 25;

    case MONS_DEEP_ELF_CONJURER:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_STORM_DRAGON:
        return 26;

    case MONS_DEEP_ELF_PRIEST:
    case MONS_GLOWING_SHAPESHIFTER:
    case MONS_TENTACLED_MONSTROSITY:
        return 27;

    case MONS_ANCIENT_LICH:
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_GOLDEN_DRAGON:
    case MONS_IRON_DRAGON:
    case MONS_QUICKSILVER_DRAGON:
    case MONS_SHADOW_DRAGON:
    case MONS_SKELETAL_DRAGON:
    case MONS_TITAN:
        return 30;

    case MONS_BIG_FISH:
    case MONS_ELECTRICAL_EEL:
    case MONS_GIANT_GOLDFISH:
    case MONS_JELLYFISH:
    case MONS_LAVA_FISH:
    case MONS_LAVA_SNAKE:
    case MONS_LAVA_WORM:
    case MONS_SWAMP_WORM:
    case MONS_WATER_ELEMENTAL:
        return 500;

    default:
        return 99;
    }
}                               // end mons_standard_level()

static int mons_standard_rare(int mcls)
{
    switch (mcls)
    {
// "another lava thing" has no stats!  (GDL)
//    case MONS_ANOTHER_LAVA_THING:
    case MONS_BIG_FISH:
    case MONS_ELECTRICAL_EEL:
    case MONS_GIANT_GOLDFISH:
    case MONS_JELLYFISH:
    case MONS_LAVA_FISH:
    case MONS_LAVA_SNAKE:
    case MONS_LAVA_WORM:
    case MONS_SWAMP_WORM:
    case MONS_WATER_ELEMENTAL:
    case MONS_SALAMANDER:
        return 500;

    case MONS_GIANT_BAT:
    case MONS_GIANT_FROG:
    case MONS_GOBLIN:
    case MONS_HILL_GIANT:
    case MONS_HOBGOBLIN:
    case MONS_IMP:
    case MONS_KOBOLD:
    case MONS_SKELETON_LARGE:
    case MONS_ORC:
    case MONS_RAT:
    case MONS_RED_DEVIL:
    case MONS_SKELETON_SMALL:
    case MONS_UGLY_THING:
    case MONS_ZOMBIE_LARGE:
    case MONS_ZOMBIE_SMALL:
        return 99;

    case MONS_CENTAUR_WARRIOR:
    case MONS_GIANT_ANT:
    case MONS_SNAKE:
        return 80;

    case MONS_FLYING_SKULL:
    case MONS_SLIME_CREATURE:
        return 75;

    case MONS_HELL_HOUND:
        return 71;

    case MONS_CENTAUR:
    case MONS_CYCLOPS:
    case MONS_GIANT_BROWN_FROG:
    case MONS_HELLION:
    case MONS_HOUND:
    case MONS_OGRE:
    case MONS_ORC_WARRIOR:
    case MONS_TROLL:
    case MONS_YAK:
    case MONS_YAKTAUR_CAPTAIN:
        return 70;

    case MONS_JELLY:
    case MONS_ORC_KNIGHT:
    case MONS_ROTTING_DEVIL:
        return 60;

    case MONS_SHAPESHIFTER:
        return 59;

    case MONS_STONE_GIANT:
        return 53;

    case MONS_BIG_KOBOLD:
    case MONS_GIANT_BEETLE:
    case MONS_GIANT_BLOWFLY:
    case MONS_GIANT_COCKROACH:
    case MONS_GIANT_GECKO:
    case MONS_GIANT_IGUANA:
    case MONS_GIANT_NEWT:
    case MONS_HIPPOGRIFF:
    case MONS_HYDRA:
    case MONS_ICE_BEAST:
    case MONS_KILLER_BEE:
    case MONS_ORC_WIZARD:
    case MONS_QUOKKA:
    case MONS_SCORPION:
    case MONS_TORMENTOR:
    case MONS_UNSEEN_HORROR:
    case MONS_WORM:
        return 50;

    case MONS_ROCK_TROLL:
        return 48;

    case MONS_MANTICORE:
    case MONS_OGRE_MAGE:
        return 45;

    case MONS_HUNGRY_GHOST:
    case MONS_SHADOW:
        return 41;

    case MONS_GIANT_CENTIPEDE:
    case MONS_GIANT_EYEBALL:
    case MONS_GIANT_SPORE:
    case MONS_GRIFFON:
    case MONS_HAIRY_DEVIL:
    case MONS_JACKAL:
    case MONS_MOTTLED_DRAGON:
    case MONS_PHANTOM:
    case MONS_REAPER:
    case MONS_TWO_HEADED_OGRE:
    case MONS_WIGHT:
    case MONS_WRAITH:
    case MONS_WYVERN:
    case MONS_YAKTAUR:
        return 40;

    case MONS_WOLF_SPIDER:
        return 36;

    case MONS_FREEZING_WRAITH:
    case MONS_GIANT_AMOEBA:
    case MONS_GILA_MONSTER:
    case MONS_GLOWING_SHAPESHIFTER:
    case MONS_SOLDIER_ANT:
        return 35;

    case MONS_BOULDER_BEETLE:
        return 34;

    case MONS_EYE_OF_DRAINING:
        return 33;

    case MONS_GIANT_SLUG:
        return 32;

    case MONS_ARMOUR_MIMIC:
    case MONS_BROWN_SNAKE:
    case MONS_DRAGON:
    case MONS_ETTIN:
    case MONS_FIRE_VORTEX:
    case MONS_GIANT_LIZARD:
    case MONS_GIANT_MITE:
    case MONS_GNOLL:
    case MONS_GOLD_MIMIC:
    case MONS_KOMODO_DRAGON:
    case MONS_MUMMY:
    case MONS_NECROPHAGE:
    case MONS_POTION_MIMIC:
    case MONS_SCROLL_MIMIC:
    case MONS_SKELETAL_WARRIOR:
    case MONS_SMALL_SNAKE:
    case MONS_SOUL_EATER:
    case MONS_SPINY_WORM:
    case MONS_VAMPIRE:
    case MONS_WEAPON_MIMIC:
    case MONS_YELLOW_WASP:
        return 30;

    case MONS_FLAYED_GHOST:
        return 29;

    case MONS_BRAIN_WORM:
        return 26;

    case MONS_BOGGART:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_TROLL:
    case MONS_FIRE_GIANT:
    case MONS_FROST_GIANT:
    case MONS_GREAT_ORB_OF_EYES:
    case MONS_IRON_TROLL:
    case MONS_OOZE:
    case MONS_ORC_PRIEST:
    case MONS_PLANT:
    case MONS_RED_WASP:
    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
        return 25;

    case MONS_BUTTERFLY:
    case MONS_FUNGUS:
    case MONS_GIANT_SNAIL:
    case MONS_ICE_DRAGON:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_RAKSHASA:
    case MONS_REDBACK:
    case MONS_SHADOW_DRAGON:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_SPHINX:
    case MONS_STEAM_DRAGON:
    case MONS_STORM_DRAGON:
    case MONS_VERY_UGLY_THING:
    case MONS_WIZARD:
        return 20;

    case MONS_BORING_BEETLE:
    case MONS_LICH:
    case MONS_TENTACLED_MONSTROSITY:
        return 17;

    case MONS_BLINK_FROG:
    case MONS_CLAY_GOLEM:
    case MONS_EFREET:
    case MONS_EYE_OF_DEVASTATION:
    case MONS_NECROMANCER:
    case MONS_WOOD_GOLEM:
        return 15;

    case MONS_KOBOLD_DEMONOLOGIST:
        return 13;

    case MONS_BUMBLEBEE:
    case MONS_ORC_HIGH_PRIEST:
        return 12;

    case MONS_DEEP_ELF_SOLDIER:
    case MONS_GIANT_ORANGE_BRAIN:
    case MONS_HELL_KNIGHT:
    case MONS_IRON_GOLEM:
    case MONS_OKLOB_PLANT:
    case MONS_ORC_SORCERER:
    case MONS_SHADOW_WRAITH:
    case MONS_STONE_GOLEM:
    case MONS_TITAN:
    case MONS_WANDERING_MUSHROOM:
        return 10;

    case MONS_GOLDEN_DRAGON:
    case MONS_ORC_WARLORD:
        return 7;

    case MONS_GARGOYLE:
        return 6;

    case MONS_CRYSTAL_GOLEM:
    case MONS_DANCING_WEAPON:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_SUMMONER:
    case MONS_IRON_DRAGON:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_SKELETAL_DRAGON:
    case MONS_QUICKSILVER_DRAGON:
    case MONS_VAPOUR:
        return 5;

    case MONS_AIR_ELEMENTAL:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_EARTH_ELEMENTAL:
    case MONS_FIRE_ELEMENTAL:
        return 4;

    case MONS_ANCIENT_LICH:
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_GUARDIAN_NAGA:
        return 3;

    case MONS_PULSATING_LUMP:
    case MONS_SHINING_EYE:
    case MONS_TOENAIL_GOLEM:
        return 2;

    default:
        return 0;
    }
}                               // end mons_standard_rare()

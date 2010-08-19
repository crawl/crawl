/*
 *  File:       zotdef.cc
 *  Summary:    Zot Def specific functions
 *  Written by: Mark Mackey
 */

#include "AppHdr.h"

#include "branch.h"
#include "directn.h"
#include "dungeon.h" // for Zotdef unique placement
#include "env.h"
#include "externs.h"
#include "ghost.h"
#include "items.h" // // for find_floor_item
#include "itemname.h" // // for make_name
#include "lev-pand.h"
#include "los.h"
#include "makeitem.h"
#include "message.h"
#include "mgen_data.h"
#include "mon-stuff.h"
#include "mon-place.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "player.h"
#include "religion.h"
#include "random.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "view.h"
#include "zotdef.h"

// Size of the mons_alloc array (or at least the bit of
// it that we use).
#define NMONSALLOC 20
#define NSLOTS (NMONSALLOC - 1)
#define BOSS_SLOT NSLOTS

//#define DEBUG_WAVE 1

monster_type pick_unique(int lev);

monster_type pick_unique(int lev);

static int _fuzz_mons_level(int level)
{
    if (level>1 && one_chance_in(7))
    {
        const int fuzz = random2avg(9, 2);
        return (fuzz > 4? level + fuzz - 4 : level);
    }
    return (level);
}

// Choose a random branch. Which branches may be chosen is a function of
// the wave number
branch_type zotdef_random_branch()
{ 
    int wavenum = you.num_turns / CYCLE_LENGTH;

    branch_type b = NUM_BRANCHES;
    while (b == NUM_BRANCHES)
    {
        branch_type pb=static_cast<branch_type>(random2(NUM_BRANCHES));
        bool ok=true;
        switch (pb)
        {
            case BRANCH_MAIN_DUNGEON:      ok=true;
					   // reduce freq at high levels
					   if (wavenum>40) ok=one_chance_in(2);
					   break;	

            case BRANCH_SNAKE_PIT:         ok= (wavenum>10);
					   // reduce freq at high levels
					   if (wavenum>40) ok=ok && one_chance_in(2);
					   break;	

            default:
            case BRANCH_ECUMENICAL_TEMPLE:
            case BRANCH_VAULTS:		   
            case BRANCH_VESTIBULE_OF_HELL: ok=false;break;		// vaults/vestibule same as dungeon

            case BRANCH_ORCISH_MINES:      ok = (wavenum<30); break;	// <6K turns only
            case BRANCH_ELVEN_HALLS:       ok = (wavenum>10 && wavenum<60); break; // 2K-12K turns
            case BRANCH_LAIR: 	           ok = (wavenum<40); break;	// <8K turns only
            case BRANCH_SWAMP: 	           ok = (wavenum>12 && wavenum<40); break;	// <8K turns only
            case BRANCH_SHOALS:	           ok = (wavenum>12 && wavenum<60); break;	// <12K turns only
            case BRANCH_CRYPT:	           ok = (wavenum>13); break; // >2.5K turns only
            case BRANCH_SLIME_PITS:        ok = (wavenum>20) && one_chance_in(2); break;	// >4K turns only
            case BRANCH_HIVE:	           ok=(wavenum>12 && wavenum<35);break;	// <7.5K turns only
            case BRANCH_HALL_OF_BLADES:	   ok=(wavenum>30);break;	// >6K turns only
            case BRANCH_TOMB:              ok=(wavenum>30) && one_chance_in(2);	break;
            case BRANCH_DIS:
            case BRANCH_COCYTUS:
            case BRANCH_TARTARUS:
            case BRANCH_GEHENNA:	   ok=(wavenum>40) && one_chance_in(3);break;
            case BRANCH_HALL_OF_ZOT:	   ok=(wavenum>50); break;	// >10K turns only

        }
        if (ok) b=pb;
        if (one_chance_in(4)) b=BRANCH_MAIN_DUNGEON;	// strong bias to main dungeon
    }
    return b;
}

int mon_strength(monster_type mon_type)
{
    monsterentry *mentry = get_monster_data(mon_type);
    if (!mentry) return 0;  // sanity
    int strength = (mentry->hpdice[0] * mentry->exp_mod) / 10;
    // Fix for skeletons and zombies
    switch (mon_type)
    {
        case MONS_SKELETON_SMALL:
        case MONS_ZOMBIE_SMALL:
            strength += 3;
            break;
        case MONS_SKELETON_LARGE:
        case MONS_ZOMBIE_LARGE:
            strength += 4;
            break;
        case MONS_PANDEMONIUM_DEMON:	// base init has 4HD (!)
            strength=30;
            break;
        default: break;
    }
    return strength;
}

// Fill the wave list with selections from a supplied array.
// Each array contains a monster if random2(power)>chance. Power
// is also compared to the strength of each critter to allow later
// waves to be stronger than earlier ones.
//
// Note that this fills in the boss slot as well.
void zotdef_fill_from_list(monster_type mlist[], int mlistlen, int chance, int power)
{
    for (int i=0; i<=NSLOTS; i++) {
        env.mons_alloc[i]=MONS_PROGRAM_BUG;
        if (i<NSLOTS && random2(power)<chance) continue;	// no monster this entry
        while (env.mons_alloc[i]==MONS_PROGRAM_BUG)
        {
            monster_type mon_type=mlist[random2(mlistlen)];
            if (random2((power*3)/2)>mon_strength(mon_type)) continue;	// bias away from weaker critters
            if (random2((power*3)/2)>mon_strength(mon_type)) 
                        env.mons_alloc[i]=mon_type;
            if (one_chance_in(100)) env.mons_alloc[i]=mon_type;	// occasional random pick
        }
    }
}

// Choose a boss from the supplied list. If a unique is chosen and has
// already been seen we try again. After a few tries we give up and
// leave the existing entry there.
void zotdef_choose_boss(monster_type mlist[], int mlistlen, int power)
{
    int tries=0;
    while (tries++<100)
    {
        monster_type mon_type=mlist[random2(mlistlen)];
        if (mons_is_unique(mon_type)
            && you.unique_creatures[mon_type]) continue;
        if (random2avg(power*3,2)<mon_strength(mon_type)) continue;

        // OK, take this one
        env.mons_alloc[BOSS_SLOT]=mon_type;
        break;
    }
}

void hydra_wave(int power)
{
    monster_type hydras[]={MONS_HYDRA};
    monster_type boss[]={MONS_LERNAEAN_HYDRA};
    zotdef_fill_from_list(hydras, sizeof(hydras)/sizeof(monster_type), 4, power);	// 66% full at power 12
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power*2);
    mpr("You hear a distant many-voiced hissing!",MSGCH_DANGER);
    more();
}

void fire_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("FIRE WAVE");
#endif
    monster_type firemons[]={MONS_FIRE_ELEMENTAL, MONS_FIRE_DRAKE, MONS_IMP,
        MONS_DRAGON, MONS_FIRE_VORTEX ,MONS_FIRE_GIANT, MONS_HELLION,
        MONS_MOLTEN_GARGOYLE, MONS_SALAMANDER, MONS_SUN_DEMON,
        MONS_RED_DRACONIAN, MONS_MOTTLED_DRACONIAN, MONS_DRACONIAN_SCORCHER,
        MONS_FLAMING_CORPSE, MONS_MOTTLED_DRAGON, MONS_EFREET,
        MONS_HELL_KNIGHT, MONS_FIEND, MONS_BALRUG, MONS_HELL_HOUND,
        MONS_HELL_HOG};
    monster_type boss[]={MONS_AZRAEL, MONS_XTAHUA, MONS_SERPENT_OF_HELL,
		    MONS_MARGERY, MONS_FIEND, MONS_BALRUG, MONS_FIRE_GIANT};
    zotdef_fill_from_list(firemons, sizeof(firemons)/sizeof(monster_type), 0, power);	
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("You hear roaring flames in the distance!",MSGCH_DANGER);
    more();
}

void cold_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("COLD WAVE");
#endif
    monster_type coldmons[]={MONS_ICE_BEAST, MONS_AZURE_JELLY, MONS_FREEZING_WRAITH,
        MONS_WHITE_IMP, MONS_ICE_DEVIL, MONS_ICE_FIEND, MONS_WHITE_DRACONIAN,
        MONS_SIMULACRUM_SMALL, MONS_SIMULACRUM_LARGE, MONS_FROST_GIANT, 
        MONS_POLAR_BEAR, MONS_BLUE_DEVIL}; 
    monster_type boss[]={MONS_ANTAEUS, MONS_ICE_FIEND, MONS_AZURE_JELLY,
			MONS_WHITE_DRACONIAN}; 
    zotdef_fill_from_list(coldmons, sizeof(coldmons)/sizeof(monster_type), 4, power);	
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("A deadly chill settles over the dungeon!",MSGCH_DANGER);
    more();
}

void gnoll_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("GNOLL WAVE");
#endif
    monster_type gnolls[]={MONS_GNOLL, MONS_GNOLL, MONS_GNOLL,
		MONS_GNOLL, MONS_GNOLL, MONS_GNOLL, MONS_TROLL};
    monster_type boss[]={MONS_GRUM, MONS_TROLL};
    zotdef_fill_from_list(gnolls, sizeof(gnolls)/sizeof(monster_type), 0, power);	// full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("Harsh voices can be heard, coming closer!",MSGCH_DANGER);
    more();
}

void rat_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("RAT WAVE");
#endif
    monster_type rats[]={MONS_RAT, MONS_GREEN_RAT, MONS_GREY_RAT,
                MONS_ORANGE_RAT};
    monster_type boss[]={MONS_ORANGE_RAT};
    zotdef_fill_from_list(rats, sizeof(rats)/sizeof(monster_type), 0, power);	// full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("You hear distant squeaking!",MSGCH_DANGER);
    more();
}

void hound_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("HOUND WAVE");
#endif
    monster_type hounds[]={ MONS_JACKAL, MONS_HOUND, MONS_WARG, 
                MONS_WOLF, MONS_WAR_DOG };
    monster_type boss[]={MONS_HELL_HOUND};
    zotdef_fill_from_list(hounds, sizeof(hounds)/sizeof(monster_type), 0, power);	// full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("Horrible howls echo around!",MSGCH_DANGER);
    more();
}

void abomination_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("ABOMINATION WAVE");
#endif
    monster_type aboms[]={ MONS_ABOMINATION_SMALL, MONS_ABOMINATION_LARGE};
    monster_type boss[]={MONS_TENTACLED_MONSTROSITY};
    zotdef_fill_from_list(aboms, sizeof(aboms)/sizeof(monster_type), 0, power);	// full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("A dreadful chittering sound fills the air. It's coming closer...",MSGCH_DANGER);
    more();
}

void ugly_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("UGLY WAVE");
#endif
    monster_type ugly[]={ MONS_UGLY_THING, MONS_UGLY_THING, MONS_UGLY_THING,
			MONS_VERY_UGLY_THING};
    monster_type boss[]={MONS_VERY_UGLY_THING};
    zotdef_fill_from_list(ugly, sizeof(ugly)/sizeof(monster_type), 6, power);	// reduced size
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("You feel uneasy.",MSGCH_DANGER);
    more();
}

void golem_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("GOLEM WAVE");
#endif
    monster_type golems[]={ MONS_CLAY_GOLEM, MONS_WOOD_GOLEM, MONS_STONE_GOLEM,
	    MONS_IRON_GOLEM, MONS_CRYSTAL_GOLEM, MONS_TOENAIL_GOLEM};
    monster_type boss[]={MONS_ELECTRIC_GOLEM};
    zotdef_fill_from_list(golems, sizeof(golems)/sizeof(monster_type), 6, power*2/3);	// reduced size
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("Booming thuds herald the arrival of something large...",MSGCH_DANGER);
    more();
}

void human_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("HUMAN WAVE");
#endif
    monster_type humans[]={ MONS_HUMAN, MONS_HELL_KNIGHT, MONS_NECROMANCER,
	    MONS_WIZARD, MONS_VAULT_GUARD, MONS_KILLER_KLOWN};
    monster_type boss[]={ MONS_HELL_KNIGHT, MONS_KILLER_KLOWN,
	    MONS_VAULT_GUARD, MONS_JOSEPH, MONS_ERICA, MONS_JOSEPHINE,
	    MONS_HAROLD, MONS_NORBERT, MONS_JOZEF, MONS_AGNES,
	    MONS_MAUD, MONS_LOUISE, MONS_FRANCIS, MONS_FRANCES,
	    MONS_RUPERT, MONS_KIRKE, MONS_WAYNE, MONS_DUANE,
	    MONS_NORRIS, MONS_FREDERICK, MONS_MARGERY, MONS_EUSTACHIO,
	    MONS_MAURICE};
    zotdef_fill_from_list(humans, sizeof(humans)/sizeof(monster_type), 4, power);	// reduced size due to banding

    // Get too many hell knights with the defaults, due to their large band size.
    // Reduce here
    for (int i=0; i<NSLOTS; i++) {
        if (env.mons_alloc[i]==MONS_HELL_KNIGHT && random2(power)<8) 
            env.mons_alloc[i]=MONS_PROGRAM_BUG;
    }

    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("War cries fill the air!",MSGCH_DANGER);
    more();
}

void butterfly_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("BUTTERFLY WAVE");
#endif
    monster_type bfs[]={ MONS_BUTTERFLY};
    zotdef_fill_from_list(bfs, sizeof(bfs)/sizeof(monster_type), 0, power);	// full
    mpr("You feel a sudden sense of peace!",MSGCH_DANGER);
    more();
}

void beast_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("BEAST WAVE");
#endif
    monster_type bst[]={ MONS_BEAST};
    zotdef_fill_from_list(bst, sizeof(bst)/sizeof(monster_type), 0, power);	// full
    mpr("A hideous howling noise can be heard in the distance!",MSGCH_DANGER);
    more();
}

void frog_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("FROG WAVE");
#endif
    monster_type frogs[]={ MONS_GIANT_FROG, MONS_GIANT_TOAD,
		MONS_SPINY_FROG, MONS_BLINK_FROG};
    monster_type boss[]={MONS_PRINCE_RIBBIT, MONS_SPINY_FROG, MONS_BLINK_FROG};
    zotdef_fill_from_list(frogs, sizeof(frogs)/sizeof(monster_type), 0, power);	// full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("Croaking noises echo off the walls!",MSGCH_DANGER);
    more();
}

void bear_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("BEAR WAVE");
#endif
    monster_type bears[]={ MONS_BEAR, MONS_GRIZZLY_BEAR, MONS_POLAR_BEAR,
		 MONS_BLACK_BEAR};
    monster_type boss[]={MONS_GRIZZLY_BEAR, MONS_POLAR_BEAR};
    zotdef_fill_from_list(bears, sizeof(bears)/sizeof(monster_type), 0, power);	// full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("Gravelly voices can be heard calling for porridge!",MSGCH_DANGER);
    more();
}

void wraith_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("WRAITH WAVE");
#endif
    monster_type wraiths[]={ MONS_WRAITH, MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH,
		MONS_SPECTRAL_WARRIOR, MONS_SPECTRAL_THING };
    monster_type boss[]={MONS_SPECTRAL_WARRIOR, MONS_SPECTRAL_THING};
    zotdef_fill_from_list(wraiths, sizeof(wraiths)/sizeof(monster_type), 0, power);	// full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("The hair rises on the back of your neck!",MSGCH_DANGER);
    more();
}

void giant_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("GIANT WAVE");
#endif
    monster_type giants[]={MONS_ETTIN, MONS_CYCLOPS, MONS_TWO_HEADED_OGRE,
	    MONS_OGRE, MONS_TROLL, MONS_MINOTAUR, MONS_HILL_GIANT,
	    MONS_STONE_GIANT, MONS_FIRE_GIANT, MONS_FROST_GIANT, MONS_OGRE_MAGE,
	    MONS_ROCK_TROLL, MONS_IRON_TROLL, MONS_DEEP_TROLL, MONS_TITAN};
    monster_type boss[]={ MONS_EROLCHA, MONS_POLYPHEMUS, MONS_ANTAEUS,
	    MONS_SNORG, MONS_PURGY, MONS_STONE_GIANT, MONS_FIRE_GIANT,
	    MONS_FROST_GIANT, MONS_TITAN};
    zotdef_fill_from_list(giants, sizeof(giants)/sizeof(monster_type), 0, power);	// full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("The stamp of enormous boots can be heard in the distance.",MSGCH_DANGER);
    more();
}

void yak_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("YAK WAVE");
#endif
    monster_type yaks[]={ MONS_SHEEP, MONS_YAK, MONS_DEATH_YAK,
		MONS_SHEEP, MONS_YAK, MONS_DEATH_YAK,
		MONS_SHEEP, MONS_YAK, MONS_DEATH_YAK,
		MONS_CYCLOPS};
    monster_type boss[]={MONS_POLYPHEMUS, MONS_CYCLOPS};
    zotdef_fill_from_list(yaks, sizeof(yaks)/sizeof(monster_type), 0, power);	// full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("Bleats and roars echo around!",MSGCH_DANGER);
    more();
}

void insect_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("INSECT WAVE");
#endif
    monster_type insects[]={MONS_GIANT_ANT, MONS_KILLER_BEE, MONS_YELLOW_WASP,
		MONS_GIANT_BEETLE, MONS_QUEEN_BEE, MONS_WOLF_SPIDER, MONS_BUTTERFLY,
		MONS_BOULDER_BEETLE, MONS_GIANT_MITE, MONS_BUMBLEBEE, MONS_REDBACK,
		MONS_GIANT_BLOWFLY, MONS_RED_WASP, MONS_SOLDIER_ANT, MONS_QUEEN_ANT,
		MONS_GIANT_COCKROACH, MONS_BORING_BEETLE, MONS_TRAPDOOR_SPIDER,
		MONS_SCORPION, MONS_GIANT_MOSQUITO, MONS_GIANT_CENTIPEDE};
    monster_type boss[]={MONS_GIANT_BEETLE, MONS_BOULDER_BEETLE,
		MONS_QUEEN_ANT, MONS_BORING_BEETLE, MONS_QUEEN_BEE};
    zotdef_fill_from_list(insects, sizeof(insects)/sizeof(monster_type), 0, power);  // full
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    mpr("You hear an ominous buzzing.",MSGCH_DANGER);
    more();
}

void pan_wave(int power)
{
#ifdef DEBUG_WAVE
    mpr("PAN WAVE");
#endif
    // The unique '&'s are a bit too strong at lower levels. Lom
    // Lobon in particular is almost unkillable
    monster_type boss[]={MONS_MNOLEG, MONS_LOM_LOBON, MONS_CEREBOV,
		MONS_GLOORX_VLOQ, MONS_GERYON, MONS_DISPATER,
		MONS_ASMODEUS, MONS_ERESHKIGAL, MONS_PANDEMONIUM_DEMON};
    monster_type weakboss[]={ MONS_PANDEMONIUM_DEMON, MONS_FIEND,
		MONS_PIT_FIEND, MONS_ICE_FIEND, MONS_BLUE_DEATH };

    for (int i=0; i<=NSLOTS; i++) {
	env.mons_alloc[i]=MONS_PROGRAM_BUG;
	while (env.mons_alloc[i]==MONS_PROGRAM_BUG)
	{
	    monster_type mon_type = static_cast<monster_type>(random2(NUM_MONSTERS));
	    monsterentry *mentry = get_monster_data(mon_type);
	    int pow=random2avg(power,2);
	    switch (mentry->showchar)
	    {
            case '5': if (pow>4) continue; break;
            case '4': if (pow>4) continue; break;
            case '3': if (pow>6) continue; break;
            case '2': if (pow>10) continue; break;
            case '1': if (pow>12) continue; break;
            default: continue;
	    }
	    env.mons_alloc[i]=mon_type;
	}
    }
    zotdef_choose_boss(boss, sizeof(boss)/sizeof(monster_type), power);
    // Weak bosses only at lower power
    if (power<27)
        zotdef_choose_boss(weakboss, sizeof(weakboss)/sizeof(monster_type), power);
    mpr("Hellish voices call for your blood. They are coming!",MSGCH_DANGER);
    more();
}

	
void zotdef_set_special_wave(int power)
{
    void (*wave_fn)(int)=NULL;
    int tries=0;

    while (wave_fn==NULL && tries++<10000)
    {
        int wpow=0;
        switch(random2(21)) 
        {
            case 0: wave_fn=hydra_wave; wpow=10; break;
            case 1: wave_fn=fire_wave; wpow=12; break;
            case 2: wave_fn=cold_wave; wpow=12; break;
            case 3: wave_fn=gnoll_wave; wpow=4; break;
            case 4: wave_fn=rat_wave; wpow=2; break;
            case 5: wave_fn=hound_wave; wpow=2; break;
            case 6: wave_fn=abomination_wave; wpow=12; break;
            case 7: wave_fn=ugly_wave; wpow=14; break;
            case 8: wave_fn=golem_wave; wpow=22; break;
            case 9: wave_fn=human_wave; wpow=12; break;
            case 10: wave_fn=butterfly_wave; wpow=1; break;
            case 11: wave_fn=beast_wave; wpow=12; break;
            case 12: wave_fn=frog_wave; wpow=4; break;
            case 13: wave_fn=bear_wave; wpow=6; break;
            case 14: wave_fn=wraith_wave; wpow=8; break;
            case 15: wave_fn=giant_wave; wpow=16; break;
            case 16: wave_fn=yak_wave; wpow=12; break; // lots of bands
            case 17: wave_fn=insect_wave; wpow=4; break;
            case 18: wave_fn=pan_wave; wpow=24; break;
            // extra copies of fire and cold at higher power
            case 19: wave_fn=fire_wave; wpow=20; break;
            case 20: wave_fn=cold_wave; wpow=20; break;
        }
        // Algorithm: doesn't appear before 'wpow-5'. Max probability
        // at 'wpow'. Doesn't appear after 'wpow*2+4'.
        // OK: do we keep this one?
        if (power>=(wpow-5) && power<=(wpow*2+4)) {	
          int diff = power-wpow;
          if (diff > 0) diff /= 2;	// weaker waves more common
          if (one_chance_in(diff*diff)) break;	// keep this one
        }
        // Nope, don't keep
        wave_fn=NULL;
    }
    if (wave_fn) wave_fn(power);
}

void debug_waves() 
{
  for (int i=0; i<15*7; i++)
  {
    you.num_turns+=200;
    zotdef_set_wave();
  }
}
	

monster_type get_zotdef_monster(level_id &place, int power)
{
    monster_type mon_type;
    monster_type mon_type_ret=MONS_PROGRAM_BUG;
    for (int i = 0; i <= 10000; ++i)
    {
        int count = 0;
        int rarity;

        do
        {
            mon_type = static_cast<monster_type>(random2(NUM_MONSTERS));
            count++;
            rarity=(place.branch==NUM_BRANCHES) ? 30 : mons_rarity(mon_type, place);
        }
        while (rarity == 0 && count < 2000);

        if (count == 2000)
            return (MONS_PROGRAM_BUG);

        // Calculate strength
        monsterentry *mentry = get_monster_data(mon_type);
        if (!mentry) continue;	// sanity
        if (mentry==get_monster_data(MONS_PROGRAM_BUG)) continue;	// sanity
        if (mons_is_unique(mon_type)) continue;	// No uniques here!
        if (mons_class_is_stationary(mon_type)) continue;	// Must be able to move!

        int strength = mon_strength(mon_type);

        // get default level
        int lev_mons = (place.branch==NUM_BRANCHES) ? ((strength*3)/2) : mons_level(mon_type, place);

        // if >50, bail out - these are special flags
        if (lev_mons>=50) continue;

        int orig_lev_mons=lev_mons;

        // adjust level based on strength, as weak monsters with high
        // level pop up on some branches and we want to allow them
        if (place.branch!=BRANCH_MAIN_DUNGEON
            && lev_mons>power 
            && lev_mons>strength*3)
        {
            lev_mons=(lev_mons+2*strength)/3;
        }

        // reduce power to 32 if that reduces diff
        if (lev_mons<=32 && power>32) power=32;
            int diff   = power - lev_mons;
        if (power>20) diff = (diff * 20) / power;	// reduce diff at high power

        int chance = rarity - (diff * diff);
        // Occasionally accept a weaker monster
        if (diff>0 && chance<=0) 
        {
            chance=1;
            if (lev_mons>20) chance=3;
            if (lev_mons>25) chance=5;
        }

        // Rarely accept monsters too far outside the power range
        if ((diff<-5 || diff > 5) && !one_chance_in(3)) continue;

        // Less OOD allowed on early levels
        if (diff<std::min(-3,-power)) continue;

        const char *bn="RANDOM";
        if (place.branch!=NUM_BRANCHES)
        {
            bn=branches[place.branch].shortname;
        }
        
        if (random2avg(100, 2) <= chance)
        {
            //mprf("ZOTDEF %d %s chose monster %s rarity %d power %d strength %d level %d chance %d",i,bn,mentry->name, rarity, power, strength, lev_mons, chance);
            mon_type_ret=mon_type;
            break;
        }

        if (i == 10000)
        {
            return (MONS_PROGRAM_BUG);
        }
    }
    
    return mon_type_ret;
}

void zotdef_set_random_branch_wave(int power)
{
    //mprf("RANDOM WAVE");
    for (int i=0; i<NSLOTS; i++)
    {
       level_id l(zotdef_random_branch(),-1);
       env.mons_alloc[i]=get_zotdef_monster(l, _fuzz_mons_level(power));
    }
    level_id l(zotdef_random_branch(),-1);
    env.mons_alloc[BOSS_SLOT]=get_zotdef_monster(l, power+BOSS_MONSTER_EXTRA_POWER);
}

void zotdef_set_branch_wave(branch_type b, int power)
{
    level_id l(b,-1);
    //mprf("BRANCH WAVE: BRANCH %s",(b==NUM_BRANCHES)?"RANDOM":branches[b].shortname);
    for (int i=0; i<NSLOTS; i++)
    {
       env.mons_alloc[i]=get_zotdef_monster(l, _fuzz_mons_level(power));
    }
    env.mons_alloc[BOSS_SLOT]=get_zotdef_monster(l, power+BOSS_MONSTER_EXTRA_POWER);
}

void set_boss_unique()
{
    for (int tries=0; tries<100; tries++)
    {
        int level = random2avg(you.num_turns/CYCLE_LENGTH,2)+1;
        monster_type which_unique = pick_unique(level);

        // Sometimes, we just quit if a unique is already placed.
        if (which_unique == MONS_PROGRAM_BUG
            || you.unique_creatures[which_unique] && one_chance_in(5))
        {
            continue;
        }

        env.mons_alloc[BOSS_SLOT]= which_unique;
        break;
    }
}


// Set the env.mons_alloc data for this wave. Note that 
// mons_alloc[BOSS_SLOT] is the boss.
//
// A game lasts for 15 runes, each rune 1400 turns apart
// (assuming FREQUENCY_ON_RUNES=7, CYCLE_LENGTH=200). That's
// a total of 105 waves. Set probabilities accordingly.
void zotdef_set_wave()
{
    // power ramps up from 1 to 35 over the course of the game.
    int power = (you.num_turns+CYCLE_LENGTH*2)/(CYCLE_LENGTH*3);

    // Early waves are all DUNGEON
    if (you.num_turns < CYCLE_LENGTH*4) 
    {
        zotdef_set_branch_wave(BRANCH_MAIN_DUNGEON, power);
        return;
    }

    switch (random2(5))
    {
        case 0: 
        case 1: zotdef_set_branch_wave(BRANCH_MAIN_DUNGEON, power); break;
        case 2: 
        case 3: {
                branch_type b = zotdef_random_branch();
                // HoB branch waves v. rare before 10K turns
                if (b==BRANCH_HALL_OF_BLADES && you.num_turns/CYCLE_LENGTH<50) 
                    b = zotdef_random_branch(); 
                zotdef_set_branch_wave(b, power); 
                break;
            }
        // A random mixture of monsters from across the branches
        case 4: zotdef_set_random_branch_wave(power); break;
    }

    // special waves have their own boss choices. Note that flavour
    // messages can be emitted by each individual wave type
    if (one_chance_in(8)) 
    {
        zotdef_set_special_wave(power); 
    }
    else
    {
        // Truly random wave, (crappily) signalled by passing branch=NUM_BRANCHES
        if (power>8 && one_chance_in(20)) 
        {
            mpr("The air ripples, and you hear distant laughter!", MSGCH_DANGER);
            more();
            zotdef_set_branch_wave(NUM_BRANCHES, power); 
        }

        // overwrite the previously-set boss with a random unique?
        if (one_chance_in(3)) set_boss_unique();
    }

/*
    for (int i=0; i<20; i++)
    {
        monsterentry *mentry = get_monster_data(env.mons_alloc[i]);
        mprf("NEW WAVE: monster %d is %s",i,mentry->name);
    }
*/
}

int zotdef_spawn(bool boss)
{

    monster_type mt=env.mons_alloc[random2(NSLOTS)];
    if (boss) 
    {
        mt=env.mons_alloc[BOSS_SLOT];
        // check if unique
        if (mons_is_unique(mt) && you.unique_creatures[mt]) 
            mt=env.mons_alloc[0];	// grab slot 0 as crap alternative
    }
    if (mt==MONS_PROGRAM_BUG) return -1;

    // Generate a monster of the appropriate branch and strength
    mgen_data mg(mt, BEH_SEEK, NULL, 0, 0, coord_def(), MHITYOU);
    mg.proximity = PROX_NEAR_STAIRS;
    mg.flags |= MG_PERMIT_BANDS;
   
    int mid = mons_place(mg);

    /* Boss monsters are named, and beefed a bit further */
    if ((mid!=-1) && boss)
    {
        // Use the proper name function: if that fails, fall back
        // to the randart name generator
        if (!menv[mid].is_named())	// Don't rename uniques!
        {
            if (!give_monster_proper_name(&menv[mid], false))
            menv[mid].mname=make_name(random_int(), false);
        }

        menv[mid].hit_points = (menv[mid].hit_points * 3) / 2;
        menv[mid].max_hit_points = menv[mid].hit_points;
    }

    return mid;
}

rune_type get_rune(int runenumber)
{
    switch (runenumber)
    {
        case 1:
            return RUNE_DIS;
        case 2:
            return RUNE_GEHENNA;
        case 3:
            return RUNE_COCYTUS;
        case 4:
            return RUNE_TARTARUS;
        case 5:
            return RUNE_SLIME_PITS;
        case 6:
            return RUNE_VAULTS;
        case 7:
            return RUNE_SNAKE_PIT;
        case 8:
            return RUNE_TOMB;
        case 9:
            return RUNE_SWAMP;
        case 10:
            return RUNE_DEMONIC;
        case 11:
            return RUNE_ABYSSAL;
        case 12:
            return RUNE_MNOLEG;
        case 13:
            return RUNE_LOM_LOBON;
        case 14:
            return RUNE_CEREBOV;
        case 15:
            return RUNE_GLOORX_VLOQ;
    }
    return RUNE_DEMONIC;
}


//       Dowan is automatically placed together with Duvessa.
static monster_type _choose_unique_by_depth(int step)
{
    int ret;
    switch (step)
    {
    case 0: // depth <= 3
        ret = random_choose(MONS_TERENCE, MONS_JESSICA, MONS_IJYB,
                            MONS_SIGMUND, -1);
        break;
    case 1: // depth <= 7
        ret = random_choose(MONS_IJYB, MONS_SIGMUND, MONS_BLORK_THE_ORC,
                            MONS_EDMUND, MONS_PRINCE_RIBBIT, MONS_PURGY,
                            MONS_MENKAURE, MONS_DUVESSA, -1);
        break;
    case 2: // depth <= 9
        ret = random_choose(MONS_BLORK_THE_ORC, MONS_EDMUND, MONS_PSYCHE,
                            MONS_EROLCHA, MONS_PRINCE_RIBBIT, MONS_GRUM,
                            MONS_GASTRONOK, MONS_MAURICE, -1);
        break;
    case 3: // depth <= 13
        ret = random_choose(MONS_PSYCHE, MONS_EROLCHA, MONS_DONALD, MONS_URUG,
                            MONS_EUSTACHIO, MONS_SONJA, MONS_GRUM,
                            MONS_JOSEPH, MONS_ERICA, MONS_JOSEPHINE, MONS_JOZEF,
                            MONS_HAROLD, MONS_NORBERT, MONS_GASTRONOK,
                            MONS_MAURICE, -1);
        break;
    case 4: // depth <= 16
        ret = random_choose(MONS_URUG, MONS_EUSTACHIO, MONS_SONJA,
                            MONS_SNORG, MONS_ERICA, MONS_JOSEPHINE, MONS_HAROLD,
                            MONS_ROXANNE, MONS_RUPERT, MONS_NORBERT, MONS_JOZEF,
                            MONS_AZRAEL, MONS_NESSOS, MONS_AGNES,
                            MONS_MAUD, MONS_LOUISE, MONS_NERGALLE, MONS_KIRKE, -1);
        break;
    case 5: // depth <= 19
        ret = random_choose(MONS_SNORG, MONS_LOUISE, MONS_FRANCIS, MONS_FRANCES,
                            MONS_RUPERT, MONS_WAYNE, MONS_DUANE, MONS_NORRIS,
                            MONS_AZRAEL, MONS_NESSOS, MONS_NERGALLE,
                            MONS_ROXANNE, MONS_SAINT_ROKA, MONS_KIRKE, -1);
        break;
    case 6: // depth > 19
    default:
        ret = random_choose(MONS_FRANCIS, MONS_FRANCES, MONS_WAYNE, MONS_DUANE,
                            MONS_XTAHUA, MONS_NORRIS, MONS_FREDERICK,
                            MONS_MARGERY, MONS_BORIS, MONS_SAINT_ROKA, -1);
    }

    return static_cast<monster_type>(ret);
}

monster_type pick_unique(int lev)
{
    // Pick generic unique depending on depth.
    int which_unique =
        ((lev <=  3) ? _choose_unique_by_depth(0) :
         (lev <=  7) ? _choose_unique_by_depth(1) :
         (lev <=  9) ? _choose_unique_by_depth(2) :
         (lev <= 13) ? _choose_unique_by_depth(3) :
         (lev <= 16) ? _choose_unique_by_depth(4) :
         (lev <= 19) ? _choose_unique_by_depth(5) :
                       _choose_unique_by_depth(6));

    return static_cast<monster_type>(which_unique);
}

// Ask for a location and place a trap there. Returns true
// for success
bool create_trap(trap_type spec_type)
{
    dist abild;
    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.may_target_monster = false;
    args.top_prompt="Make ";
    args.top_prompt+=trap_name(spec_type);
    args.top_prompt+=" trap where?";
    direction(abild, args);
    const dungeon_feature_type grid = grd(abild.target);
    if (!abild.isValid)
    {
        if (abild.isCancel)
            canned_msg(MSG_OK);
        return (false);
    }
    // only try to create on floor squares 
    if (grid >= DNGN_FLOOR_MIN && grid <= DNGN_FLOOR_MAX)
    {
        return place_specific_trap(abild.target, spec_type);
    }
    else
    {
        mpr("You can't create a trap there!");
        return (false);
    }
}

bool create_zotdef_ally(monster_type mtyp, const char *successmsg)
{
    dist abild;
    std::string msg="Make ";
    msg+=get_monster_data(mtyp)->name;
    msg+=" where?";

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.may_target_monster = false;
    args.top_prompt=msg;
    direction(abild, args);

    if (!abild.isValid)
    {
        if (abild.isCancel)
            canned_msg(MSG_OK);
        return (false);
    }
    if (mons_place( mgen_data(mtyp, BEH_FRIENDLY, &you, 0, 0, abild.target, you.pet_target)) != -1)
    {
        mpr(successmsg);
        return (true);
    }
    else
    {
       mpr("You can't create it there!");
       return (false);
    }
}



/**
 * @file
 * @brief Zot Defence specific functions
**/

#include "AppHdr.h"
#include "bitary.h"

#include "branch.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h" // for Zotdef unique placement
#include "env.h"
#include "externs.h"
#include "files.h"
#include "godprayer.h"
#include "items.h"
#include "itemname.h" // for make_name
#include "itemprop.h"
#include "makeitem.h"
#include "message.h"
#include "mgen_data.h"
#include "mon-stuff.h"
#include "mon-place.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "place.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "libutil.h"
#include "zotdef.h"

// Size of the mons_alloc array (or at least the bit of
// it that we use).
#define NSLOTS (MAX_MONS_ALLOC - 1)
#define BOSS_SLOT NSLOTS
#define END static_cast<monster_type>(-1)

static monster_type _pick_unique(int level);

static int _fuzz_mons_depth(int level)
{
    if (level > 1 && one_chance_in(7))
    {
        const int fuzz = random2avg(9, 2);
        return (fuzz > 4 ? level + fuzz - 4 : level);
    }
    return level;
}

// Choose a random branch. Which branches may be chosen is a function of
// the wave number
static branch_type _zotdef_random_branch()
{
    int wavenum = you.num_turns / ZOTDEF_CYCLE_LENGTH;

    while (true)
    {
        branch_type pb = static_cast<branch_type>(random2(NUM_BRANCHES));
        bool ok = true;
        switch (pb)
        {
            case BRANCH_MAIN_DUNGEON:
                ok = true;
                // reduce freq at high levels
                if (wavenum > 40)
                    ok = coinflip();
                break;

            case BRANCH_SNAKE_PIT:
                ok = wavenum > 10;
                // reduce freq at high levels
                if (wavenum > 40 && !coinflip())
                   ok = false;
                break;

            default:
            case BRANCH_ECUMENICAL_TEMPLE:
            case BRANCH_VAULTS:
            case BRANCH_VESTIBULE_OF_HELL:
                ok = false;
                break;                // vaults/vestibule same as dungeon

            case BRANCH_ORCISH_MINES:
                ok = wavenum < 30;                 // <6K turns only
                break;
            case BRANCH_ELVEN_HALLS:
                ok = wavenum > 10 && wavenum < 60; // 2.2-12K turns
                break;
            case BRANCH_LAIR:
                ok = wavenum < 40;                 // <8K turns only
                break;
            case BRANCH_SWAMP:
                ok = wavenum > 12 && wavenum < 40; // 2.6-8K turns
                break;
            case BRANCH_SHOALS:
                ok = wavenum > 12 && wavenum < 60; // 2.6-12K turns
                break;
            case BRANCH_CRYPT:
                ok = wavenum > 13;                 // 2.8K-
                break;
            case BRANCH_SLIME_PITS:
                ok = wavenum > 20 && coinflip();   // 4K-
                break;        // >4K turns only
            case BRANCH_HALL_OF_BLADES:
                ok = wavenum > 30;                 // 6K-
                break;
            case BRANCH_TOMB:
                ok = wavenum > 30 && coinflip();   // 6K-
                break;
            case BRANCH_DIS:                       // 8K-
            case BRANCH_COCYTUS:
            case BRANCH_TARTARUS:
            case BRANCH_GEHENNA:
                ok = wavenum > 40 && one_chance_in(3);
                break;
            case BRANCH_HALL_OF_ZOT:               // 10K-
                ok = wavenum > 50;
                break;
        }
        if (ok)
            return (one_chance_in(4) ? BRANCH_MAIN_DUNGEON : pb);
            // strong bias to main dungeon
    }
}

static int _mon_strength(monster_type mon_type)
{
    const monsterentry *mentry = get_monster_data(mon_type);
    if (!mentry)
        return 0; // sanity
    int strength = (mentry->hpdice[0] * mentry->exp_mod) / 10;
    // Fix for skeletons and zombies
    switch (mon_type)
    {
        case MONS_SKELETON:
        case MONS_ZOMBIE:
            strength += 3;
            break;
        default:
            break;
    }
    return strength;
}

// Fill the wave list with selections from a supplied array.
// Each array contains a monster if random2(power)>chance. Power
// is also compared to the strength of each critter to allow later
// waves to be stronger than earlier ones.
//
// Note that this fills in the boss slot as well.
static void _zotdef_fill_from_list(monster_type mlist[], int chance, int power)
{
    int ls = 0;
    while (mlist[ls] != END)
        ls++;
    ASSERT(ls > 0);

    for (int i = 0; i <= NSLOTS; i++)
    {
        env.mons_alloc[i] = MONS_PROGRAM_BUG;
        if (i < NSLOTS && random2(power) < chance)
             continue;        // no monster this entry
        while (env.mons_alloc[i] == MONS_PROGRAM_BUG)
        {
            monster_type mon_type = mlist[random2(ls)];
            if (random2(power * 3 / 2) > _mon_strength(mon_type))
                continue;        // bias away from weaker critters
            if (random2(power * 3 / 2) > _mon_strength(mon_type))
                env.mons_alloc[i] = mon_type;
            if (one_chance_in(100))
                env.mons_alloc[i] = mon_type;      // occasional random pick
        }
    }
}

// Choose a boss from the supplied list. If a unique is chosen and has
// already been seen we try again. After a few tries we give up and
// leave the existing entry there.
static void _zotdef_choose_boss(monster_type mlist[], int power)
{
    int ls = 0;
    while (mlist[ls] != END)
        ls++;
    ASSERT(ls > 0);

    int tries = 0;
    while (tries++ < 100)
    {
        monster_type mon_type = mlist[random2(ls)];
        if (mons_is_unique(mon_type)
            && you.unique_creatures[mon_type])
        {
            continue;
        }
        if (random2avg(power * 3, 2) < _mon_strength(mon_type))
            continue;

        // OK, take this one
        env.mons_alloc[BOSS_SLOT] = mon_type;
        break;
    }
}

static void _zotdef_danger_msg(const char *msg)
{
    mpr(msg, MSGCH_DANGER);
    more();
}

static void wave_name(const char *n)
{
    you.zotdef_wave_name = n;
    dprf("%s", n);
}

static void _hydra_wave(int power)
{
    wave_name("HYDRA WAVE");
    monster_type hydras[] = {MONS_HYDRA, END};
    monster_type boss[] = {MONS_LERNAEAN_HYDRA, END};
    _zotdef_fill_from_list(hydras, 4, power); // 66% full at power 12
    _zotdef_choose_boss(boss, power * 2);
    _zotdef_danger_msg("You hear a distant many-voiced hissing!");
}

static void _fire_wave(int power)
{
    wave_name("FIRE WAVE");
    monster_type firemons[] = {MONS_FIRE_ELEMENTAL, MONS_FIRE_DRAKE, MONS_CRIMSON_IMP,
        MONS_DRAGON, MONS_FIRE_VORTEX ,MONS_FIRE_GIANT, MONS_HELLION,
        MONS_MOLTEN_GARGOYLE, MONS_SALAMANDER, MONS_SUN_DEMON,
        MONS_RED_DRACONIAN, MONS_MOTTLED_DRACONIAN, MONS_DRACONIAN_SCORCHER,
        MONS_FLAMING_CORPSE, MONS_MOTTLED_DRAGON, MONS_EFREET,
        MONS_HELL_KNIGHT, MONS_BRIMSTONE_FIEND, MONS_BALRUG, MONS_HELL_HOUND,
        MONS_HELL_HOG, END};
    monster_type boss[] = {MONS_AZRAEL, MONS_XTAHUA, MONS_SERPENT_OF_HELL,
                    MONS_MARGERY, MONS_BRIMSTONE_FIEND, MONS_BALRUG, MONS_FIRE_GIANT, END};
    _zotdef_fill_from_list(firemons, 0, power);
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("You hear roaring flames in the distance!");
}

static void _cold_wave(int power)
{
    wave_name("COLD WAVE");
    monster_type coldmons[] = {MONS_ICE_BEAST, MONS_AZURE_JELLY,
        MONS_FREEZING_WRAITH, MONS_WHITE_IMP, MONS_ICE_DEVIL, MONS_ICE_FIEND,
        MONS_WHITE_DRACONIAN, MONS_SIMULACRUM, MONS_SIMULACRUM,
        MONS_FROST_GIANT, MONS_POLAR_BEAR, MONS_BLUE_DEVIL, END};
    monster_type boss[] = {MONS_ANTAEUS, MONS_ICE_FIEND, MONS_AZURE_JELLY,
                           MONS_WHITE_DRACONIAN, END};
    _zotdef_fill_from_list(coldmons, 4, power);
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("A deadly chill settles over the dungeon!");
}

static void _gnoll_wave(int power)
{
    wave_name("GNOLL WAVE");
    monster_type gnolls[] = {MONS_GNOLL, MONS_GNOLL, MONS_GNOLL,
                MONS_GNOLL, MONS_GNOLL_SHAMAN, MONS_GNOLL_SERGEANT,
                MONS_TROLL, END};
    monster_type boss[] = {MONS_GRUM, MONS_TROLL, END};
    _zotdef_fill_from_list(gnolls, 0, power); // full
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("Harsh voices can be heard, coming closer!");
}

static void _rat_wave(int power)
{
    wave_name("RAT WAVE");
    monster_type rats[] = {MONS_RAT, MONS_GREEN_RAT, MONS_GREY_RAT,
                MONS_ORANGE_RAT, END};
    monster_type boss[] = {MONS_ORANGE_RAT, END};
    _zotdef_fill_from_list(rats, 0, power); // full power
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("You hear distant squeaking!");
}

static void _hound_wave(int power)
{
    wave_name("HOUND WAVE");
    monster_type hounds[] = {MONS_JACKAL, MONS_HOUND, MONS_WARG,
                MONS_WOLF, END};
    monster_type boss[] = {MONS_HELL_HOUND, END};
    _zotdef_fill_from_list(hounds, 0, power); // full
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("Horrible howls echo around!");
}

static void _abomination_wave(int power)
{
    wave_name("ABOMINATION WAVE");
    monster_type aboms[] = {MONS_ABOMINATION_SMALL, MONS_ABOMINATION_LARGE, END};
    monster_type boss[] = {MONS_TENTACLED_MONSTROSITY, END};
    _zotdef_fill_from_list(aboms, 0, power); // full
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("A dreadful chittering sound fills the air. It's coming closer...");
}

static void _ugly_wave(int power)
{
    wave_name("UGLY WAVE");
    monster_type ugly[] = {MONS_UGLY_THING, MONS_UGLY_THING, MONS_UGLY_THING,
                           MONS_VERY_UGLY_THING, END};
    monster_type boss[] = {MONS_VERY_UGLY_THING, END};
    _zotdef_fill_from_list(ugly, 6, power); // reduced size
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("You feel uneasy.");
}

static void _golem_wave(int power)
{
    wave_name("GOLEM WAVE");
    monster_type golems[] = {MONS_CLAY_GOLEM, MONS_STONE_GOLEM,
            MONS_IRON_GOLEM, MONS_CRYSTAL_GOLEM, MONS_TOENAIL_GOLEM, END};
    monster_type boss[] = {MONS_ELECTRIC_GOLEM, END};
    _zotdef_fill_from_list(golems, 6, power * 2 / 3); // reduced size
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("Booming thuds herald the arrival of something large...");
}

static void _human_wave(int power)
{
    wave_name("HUMAN WAVE");
    monster_type humans[] = {MONS_HUMAN, MONS_HELL_KNIGHT, MONS_NECROMANCER,
            MONS_WIZARD, MONS_VAULT_GUARD, MONS_KILLER_KLOWN, END};
    monster_type boss[] = {MONS_HELL_KNIGHT, MONS_KILLER_KLOWN,
            MONS_VAULT_GUARD, MONS_JOSEPH, MONS_ERICA, MONS_JOSEPHINE,
            MONS_HAROLD, MONS_AGNES,
            MONS_MAUD, MONS_LOUISE,  MONS_FRANCES,
            MONS_RUPERT, MONS_KIRKE,
            MONS_NORRIS, MONS_FREDERICK, MONS_MARGERY, MONS_EUSTACHIO,
            MONS_MAURICE, END};
    _zotdef_fill_from_list(humans, 4, power); // reduced size due to banding

    // Get too many hell knights with the defaults, due to their large band
    // size. Reduce here
    for (int i = 0; i < NSLOTS; i++)
    {
        if (env.mons_alloc[i] == MONS_HELL_KNIGHT && random2(power) < 8)
            env.mons_alloc[i] = MONS_PROGRAM_BUG;
    }

    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("War cries fill the air!");
}

static void _butterfly_wave(int power)
{
    wave_name("BUTTERFLY WAVE");
    monster_type bfs[] = {MONS_BUTTERFLY, END};
    _zotdef_fill_from_list(bfs, 0, power); // full
    _zotdef_danger_msg("You feel a sudden sense of peace!");
}

static void _hell_beast_wave(int power)
{
    wave_name("HELL BEAST WAVE");
    monster_type bst[] = {MONS_HELL_BEAST, END};
    _zotdef_fill_from_list(bst, 0, power); // full
    _zotdef_danger_msg("A hideous howling noise can be heard in the distance!");
}

static void _frog_wave(int power)
{
    wave_name("FROG WAVE");
    monster_type frogs[] = {MONS_GIANT_FROG, MONS_SPINY_FROG, MONS_BLINK_FROG, END};
    monster_type boss[] = {MONS_PRINCE_RIBBIT, MONS_SPINY_FROG, MONS_BLINK_FROG, END};
    _zotdef_fill_from_list(frogs, 0, power); // full
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("Croaking noises echo off the walls!");
}

static void _bear_wave(int power)
{
    wave_name("BEAR WAVE");
    monster_type bears[] = {MONS_GRIZZLY_BEAR, MONS_POLAR_BEAR,
                 MONS_BLACK_BEAR, END};
    monster_type boss[] = {MONS_GRIZZLY_BEAR, MONS_POLAR_BEAR, END};
    _zotdef_fill_from_list(bears, 0, power); // full
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("Gravelly voices can be heard calling for porridge!");
}

static void _wraith_wave(int power)
{
    wave_name("WRAITH WAVE");
    monster_type wraiths[] = {MONS_WRAITH, MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH,
                MONS_EIDOLON, MONS_PHANTASMAL_WARRIOR, MONS_SPECTRAL_THING, END};
    monster_type boss[] = {MONS_EIDOLON, MONS_PHANTASMAL_WARRIOR,
                MONS_SPECTRAL_THING, END};
    _zotdef_fill_from_list(wraiths, 0, power); // full
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("You shudder with fear!");
}

static void _giant_wave(int power)
{
    wave_name("GIANT WAVE");
    monster_type giants[] = {MONS_ETTIN, MONS_CYCLOPS, MONS_TWO_HEADED_OGRE,
            MONS_OGRE, MONS_TROLL, MONS_MINOTAUR, MONS_HILL_GIANT,
            MONS_STONE_GIANT, MONS_FIRE_GIANT, MONS_FROST_GIANT, MONS_OGRE_MAGE,
            MONS_IRON_TROLL, MONS_DEEP_TROLL, MONS_TITAN, END};
    monster_type boss[] = {MONS_EROLCHA, MONS_POLYPHEMUS, MONS_ANTAEUS,
            MONS_SNORG, MONS_PURGY, MONS_STONE_GIANT, MONS_FIRE_GIANT,
            MONS_FROST_GIANT, MONS_TITAN, END};
    _zotdef_fill_from_list(giants, 0, power); // full
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("The stamp of enormous boots can be heard in the distance.");
}

static void _yak_wave(int power)
{
    wave_name("YAK WAVE");
    monster_type yaks[] = {MONS_SHEEP, MONS_YAK, MONS_DEATH_YAK,
                MONS_SHEEP, MONS_YAK, MONS_DEATH_YAK,
                MONS_SHEEP, MONS_YAK, MONS_DEATH_YAK,
                MONS_CYCLOPS, END};
    monster_type boss[] = {MONS_POLYPHEMUS, MONS_CYCLOPS, END};
    _zotdef_fill_from_list(yaks, 0, power); // full
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("Bleats and roars echo around!");
}

static void _insect_wave(int power)
{
    wave_name("INSECT WAVE");
    monster_type insects[] = {MONS_WORKER_ANT, MONS_KILLER_BEE, MONS_YELLOW_WASP,
                MONS_GOLIATH_BEETLE, MONS_QUEEN_BEE, MONS_WOLF_SPIDER, MONS_BUTTERFLY,
                MONS_BOULDER_BEETLE, MONS_GIANT_MITE, MONS_FIREFLY, MONS_REDBACK,
                MONS_VAMPIRE_MOSQUITO, MONS_RED_WASP, MONS_SOLDIER_ANT, MONS_QUEEN_ANT,
                MONS_GIANT_COCKROACH, MONS_BORING_BEETLE, MONS_TRAPDOOR_SPIDER,
                MONS_SCORPION, MONS_GIANT_CENTIPEDE, END};
    monster_type boss[] = {MONS_GOLIATH_BEETLE, MONS_BOULDER_BEETLE,
                MONS_QUEEN_ANT, MONS_BORING_BEETLE, MONS_QUEEN_BEE, END};
    _zotdef_fill_from_list(insects, 0, power); // full
    _zotdef_choose_boss(boss, power);
    _zotdef_danger_msg("You hear an ominous buzzing.");
}

static void _pan_wave(int power)
{
    wave_name("PAN WAVE");
    // The unique '&'s are a bit too strong at lower levels. Lom
    // Lobon in particular is almost unkillable
    monster_type boss[] = {MONS_MNOLEG, MONS_LOM_LOBON, MONS_CEREBOV,
                MONS_GLOORX_VLOQ, MONS_GERYON, MONS_DISPATER,
                MONS_ASMODEUS, MONS_ERESHKIGAL, MONS_PANDEMONIUM_LORD,
                MONS_IGNACIO, END};
    monster_type weakboss[] = {MONS_PANDEMONIUM_LORD, MONS_BRIMSTONE_FIEND,
                MONS_HELL_SENTINEL, MONS_ICE_FIEND, MONS_BLIZZARD_DEMON, END};

    for (int i = 0; i <= NSLOTS; i++)
    {
        env.mons_alloc[i] = MONS_PROGRAM_BUG;
        while (env.mons_alloc[i] == MONS_PROGRAM_BUG)
        {
            monster_type mon_type = static_cast<monster_type>(random2(NUM_MONSTERS));
            const monsterentry *mentry = get_monster_data(mon_type);
            int pow = random2avg(power, 2);
            switch (mentry->basechar)
            {
            case '5': if (pow > 4) continue; break;
            case '4': if (pow > 4) continue; break;
            case '3': if (pow > 6) continue; break;
            case '2': if (pow > 10) continue; break;
            case '1': if (pow > 12) continue; break;
            default: continue;
            }
            if (mons_is_unique(mon_type))
                continue;
            env.mons_alloc[i] = mon_type;
        }
    }
    // Weak bosses only at lower power
    _zotdef_choose_boss((power < 27 ? weakboss : boss), power);
    _zotdef_danger_msg("Hellish voices call for your blood. They are coming!");
}

static void _zotdef_set_special_wave(int power)
{
    void (*wave_fn)(int) = NULL;
    int tries = 0;

    while (wave_fn == NULL && tries++ < 10000)
    {
        int wpow = 0;
        switch (random2(21))
        {
            case 0: wave_fn = _hydra_wave; wpow = 10; break;
            case 1: wave_fn = _fire_wave; wpow = 12; break;
            case 2: wave_fn = _cold_wave; wpow = 12; break;
            case 3: wave_fn = _gnoll_wave; wpow = 4; break;
            case 4: wave_fn = _rat_wave; wpow = 2; break;
            case 5: wave_fn = _hound_wave; wpow = 2; break;
            case 6: wave_fn = _abomination_wave; wpow = 12; break;
            case 7: wave_fn = _ugly_wave; wpow = 14; break;
            case 8: wave_fn = _golem_wave; wpow = 22; break;
            case 9: wave_fn = _human_wave; wpow = 12; break;
            case 10: wave_fn = _butterfly_wave; wpow = 1; break;
            case 11: wave_fn = _hell_beast_wave; wpow = 12; break;
            case 12: wave_fn = _frog_wave; wpow = 4; break;
            case 13: wave_fn = _bear_wave; wpow = 6; break;
            case 14: wave_fn = _wraith_wave; wpow = 8; break;
            case 15: wave_fn = _giant_wave; wpow = 16; break;
            case 16: wave_fn = _yak_wave; wpow = 12; break; // lots of bands
            case 17: wave_fn = _insect_wave; wpow = 4; break;
            case 18: wave_fn = _pan_wave; wpow = 24; break;
            // extra copies of fire and cold at higher power
            case 19: wave_fn = _fire_wave; wpow = 20; break;
            case 20: wave_fn = _cold_wave; wpow = 20; break;
        }
        // Algorithm: doesn't appear before 'wpow-5'. Max probability
        // at 'wpow'. Doesn't appear after 'wpow*2+4'.
        // OK: do we keep this one?
        if (power >= (wpow - 5) && power <= (wpow * 2 + 4))
        {
            int diff = power - wpow;
            if (diff > 0)
                diff /= 2;        // weaker waves more common
            if (one_chance_in(diff * diff))
                break;        // keep this one
        }
        // Nope, don't keep
        wave_fn = NULL;
    }
    if (wave_fn)
        wave_fn(power);
}

void debug_waves()
{
    // Test more than just 15 runes, the player may stay longer, and if
    // for some reason a rune is lost, he will have to, and get extra
    // demonics.
    for (int i = 0; i < 30 * ZOTDEF_RUNE_FREQ; i++)
    {
        you.num_turns += ZOTDEF_CYCLE_LENGTH;
        zotdef_set_wave();
    }
}

static monster_type _get_zotdef_monster(level_id &place, int power)
{
    monster_type mon_type;
    for (int i = 0; i <= 10000; ++i)
    {
        int rarity;

        if (place.branch == NUM_BRANCHES)
        {
            mon_type = static_cast<monster_type>(random2(NUM_MONSTERS - 1) + 1);
            rarity = 30;
        }
        else
        {
            mon_type = pick_monster_no_rarity(place.branch);
            rarity = mons_rarity(mon_type, place.branch);
            ASSERT(rarity > 0);
        }

        // Calculate strength
        const monsterentry *mentry = get_monster_data(mon_type);
        if (!mentry)
            continue;        // sanity
        if (mentry == get_monster_data(MONS_PROGRAM_BUG))
            continue;        // sanity
        if (mons_class_flag(mon_type, M_NO_POLY_TO | M_CANT_SPAWN))
            continue;
        if (mons_class_flag(mon_type, M_UNFINISHED))
            continue;
        if (mons_is_unique(mon_type))
            continue;        // No uniques here!
        if (mons_class_is_stationary(mon_type))
            continue;        // Must be able to move!
        if (mons_is_mimic(mon_type))
            continue;

        int strength = _mon_strength(mon_type);

        // get default level
        int lev_mons = (place.branch == NUM_BRANCHES)
                       ? strength * 3 / 2
                       : mons_depth(mon_type, place.branch)
                         + absdungeon_depth(place.branch, 0);

        // if >50, bail out - these are special flags
        if (lev_mons >= 50)
            continue;

        // adjust level based on strength, as weak monsters with high
        // level pop up on some branches and we want to allow them
        if (place.branch != BRANCH_MAIN_DUNGEON
            && lev_mons > power
            && lev_mons > strength * 3)
        {
            lev_mons = (lev_mons + 2 * strength) / 3;
        }

        // reduce power to 32 if that reduces diff
        if (lev_mons <= 32 && power > 32)
            power = 32;
        int diff = power - lev_mons;
        if (power > 20)
            diff = diff * 20 / power;        // reduce diff at high power

        int chance = rarity - (diff * diff);
        // Occasionally accept a weaker monster
        if (diff > 0 && chance <= 0)
        {
            chance = 1;
            if (lev_mons > 20) chance = 3;
            if (lev_mons > 25) chance = 5;
        }

        // Rarely accept monsters too far outside the power range
        if ((diff <- 5 || diff > 5) && !one_chance_in(3))
            continue;

        // Less OOD allowed on early levels
        if (diff < min(-3,-power))
            continue;

        if (random2avg(100, 2) <= chance)
        {
            dprf("ZOTDEF %d %s chose monster %s rarity %d power %d strength %d "
                 "level %d chance %d", i,
                 (place.branch == NUM_BRANCHES) ? "RANDOM"
                     : branches[place.branch].shortname,
                 mentry->name, rarity, power,
                 strength, lev_mons, chance);
            return mon_type;
        }
    }

    return MONS_PROGRAM_BUG;
}

static void _zotdef_set_random_branch_wave(int power)
{
    wave_name("RANDOM WAVE");
    for (int i = 0; i < NSLOTS; i++)
    {
        level_id l(_zotdef_random_branch(), -1);
        env.mons_alloc[i] = _get_zotdef_monster(l, _fuzz_mons_depth(power));
    }
    level_id l(_zotdef_random_branch(), -1);
    env.mons_alloc[BOSS_SLOT] = _get_zotdef_monster(l,
        power + ZOTDEF_BOSS_EXTRA_POWER);
}

static void _zotdef_set_branch_wave(branch_type b, int power)
{
    level_id l(b,-1);
    char buf[128];
    snprintf(buf, sizeof(buf), "BRANCH WAVE: BRANCH %s",
         (b == NUM_BRANCHES) ? "RANDOM" : branches[b].shortname);
    wave_name(buf);
    for (int i = 0; i < NSLOTS; i++)
        env.mons_alloc[i] = _get_zotdef_monster(l, _fuzz_mons_depth(power));
    env.mons_alloc[BOSS_SLOT] = _get_zotdef_monster(l,
                                    power + ZOTDEF_BOSS_EXTRA_POWER);
}

static void _zotdef_set_boss_unique()
{
    for (int tries = 0; tries < 100; tries++)
    {
        int level = random2avg(you.num_turns / ZOTDEF_CYCLE_LENGTH, 2) + 1;
        monster_type which_unique = _pick_unique(level);

        // Sometimes, we just quit if a unique is already placed.
        if (which_unique == MONS_PROGRAM_BUG
            || you.unique_creatures[which_unique] && one_chance_in(5))
        {
            continue;
        }

        env.mons_alloc[BOSS_SLOT] = which_unique;
        break;
    }
}

// Set the env.mons_alloc data for this wave. Note that
// mons_alloc[BOSS_SLOT] is the boss.
//
// A game lasts for 15 runes, each rune 1400 turns apart
// (assuming ZOTDEF_RUNE_FREQ=7, ZOTDEF_CYCLE_LENGTH=200). That's
// a total of 105 waves. Set probabilities accordingly.
void zotdef_set_wave()
{
    // power ramps up from 1 to 35 over the course of the game.
    int power = (you.num_turns + ZOTDEF_CYCLE_LENGTH * 2) / (ZOTDEF_CYCLE_LENGTH * 3);

    // Early waves are all DUNGEON
    if (you.num_turns < ZOTDEF_CYCLE_LENGTH * 4)
    {
        _zotdef_set_branch_wave(BRANCH_MAIN_DUNGEON, power);
        return;
    }

    switch (random2(5))
    {
    case 0:
    case 1:
        _zotdef_set_branch_wave(BRANCH_MAIN_DUNGEON, power);
        break;
    case 2:
    case 3:
    {
        branch_type b = _zotdef_random_branch();
        // HoB branch waves v. rare before 10K turns
        if (b == BRANCH_HALL_OF_BLADES && you.num_turns / ZOTDEF_CYCLE_LENGTH < 50)
            b = _zotdef_random_branch();
        _zotdef_set_branch_wave(b, power);
        break;
    }
    // A random mixture of monsters from across the branches
    case 4:
        _zotdef_set_random_branch_wave(power);
        break;
    }

    // special waves have their own boss choices. Note that flavour
    // messages can be emitted by each individual wave type
    if (one_chance_in(8))
        _zotdef_set_special_wave(power);
    else
    {
        // Truly random wave, (crappily) signalled by passing branch=NUM_BRANCHES
        if (power > 8 && one_chance_in(20))
        {
            _zotdef_danger_msg("The air ripples, and you hear distant laughter!");
            _zotdef_set_branch_wave(NUM_BRANCHES, power);
        }

        // overwrite the previously-set boss with a random unique?
        if (one_chance_in(3))
            _zotdef_set_boss_unique();
    }

    dprf("NEW WAVE: %s", zotdef_debug_wave_desc().c_str());
}

string zotdef_debug_wave_desc()
{
    string list = you.zotdef_wave_name + " [";
    for (int i = 0; i <= (crawl_state.game_is_zotdef() ? NSLOTS : 9); i++)
    {
        if (i)
            list += ", ";
        const monsterentry *mentry = get_monster_data(env.mons_alloc[i]);
        if (!env.mons_alloc[i])
            list += "EMPTY";
        else if (mentry)
            list += mentry->name;
        else
            list += make_stringf("!!!INVALID (%d)!!!", env.mons_alloc[i]);
    }
    return list + "]";
}

monster* zotdef_spawn(bool boss)
{
    monster_type mt = env.mons_alloc[random2(NSLOTS)];
    if (boss)
    {
        mt = env.mons_alloc[BOSS_SLOT];
        // check if unique
        if (mons_is_unique(mt) && you.unique_creatures[mt])
            mt = env.mons_alloc[0];        // grab slot 0 as crap alternative
    }
    if (mt == MONS_PROGRAM_BUG)
        return 0;

    // Generate a monster of the appropriate branch and strength
    mgen_data mg(mt, BEH_SEEK, NULL, 0, 0, coord_def(), MHITYOU);
    mg.proximity = PROX_NEAR_STAIRS;
    mg.flags |= MG_PERMIT_BANDS;

    // Hack: emulate old mg.power
    mg.place = level_id(BRANCH_MAIN_DUNGEON, you.num_turns / (ZOTDEF_CYCLE_LENGTH * 3) + 1);
    // but only for item generation/etc., not for actual monster selection.
    ASSERT(mt != RANDOM_MONSTER);

    monster *mon  = mons_place(mg);

    // Boss monsters which aren't uniques are named, and beefed a bit further
    if (mon && boss && !mons_is_unique(mt))
    {
        // Use the proper name function: if that fails, fall back
        // to the randart name generator
        if (!mon->is_named())        // Don't rename uniques!
        {
            if (!give_monster_proper_name(mon, false))
                mon->mname = make_name(random_int(), false);

            mon->props["dbname"].get_string() = mons_class_name(mt);
        }

        mon->hit_points = mon->hit_points * 3 / 2;
        mon->max_hit_points = mon->hit_points;
    }

    return mon;
}

static rune_type _get_rune()
{
    FixedBitVector<NUM_RUNE_TYPES> runes = you.runes;
    for (int i = 0; i < MAX_ITEMS; i++)
        if (item_is_rune(mitm[i]))
            runes.set(mitm[i].plus);
    int already = 0;
    for (int i = 0; i < NUM_RUNE_TYPES; i++)
        if (runes[i])
            already++;

    if (already >= 15) // don't allow sitting for all 18/19
        return RUNE_DEMONIC;

    rune_type rune;
    do rune = (rune_type)random2(NUM_RUNE_TYPES);
    while (runes[rune] || rune == RUNE_FOREST || rune == RUNE_DEMONIC);

    return rune;
}

// Dowan is automatically placed together with Duvessa.
static monster_type _choose_unique_by_depth(int step)
{
    monster_type ret;
    switch (step)
    {
    case 0: // depth <= 3
        ret = random_choose(MONS_TERENCE, MONS_JESSICA, MONS_IJYB,
                            MONS_SIGMUND, -1);
        break;
    case 1: // depth <= 7
        ret = random_choose(MONS_IJYB, MONS_SIGMUND, MONS_BLORK_THE_ORC,
                            MONS_EDMUND, MONS_PRINCE_RIBBIT, MONS_PURGY,
                            MONS_MENKAURE, MONS_DUVESSA, MONS_PIKEL, -1);
        break;
    case 2: // depth <= 9
        ret = random_choose(MONS_BLORK_THE_ORC, MONS_EDMUND, MONS_PSYCHE, MONS_JOSEPH,
                            MONS_EROLCHA, MONS_PRINCE_RIBBIT, MONS_GRUM,
                            MONS_GASTRONOK, MONS_GRINDER, MONS_MAURICE,
                            MONS_PIKEL, -1);
        break;
    case 3: // depth <= 13
        ret = random_choose(MONS_PSYCHE, MONS_EROLCHA, MONS_DONALD, MONS_URUG,
                            MONS_EUSTACHIO, MONS_SONJA, MONS_GRUM, MONS_NIKOLA,
                            MONS_ERICA, MONS_JOSEPHINE,
                            MONS_HAROLD, MONS_GASTRONOK, MONS_ILSUIW,
                            MONS_MAURICE, -1);
        break;
    case 4: // depth <= 16
        ret = random_choose(MONS_URUG, MONS_EUSTACHIO, MONS_SONJA,
                            MONS_SNORG, MONS_ERICA, MONS_JOSEPHINE, MONS_HAROLD,
                            MONS_ROXANNE, MONS_RUPERT, MONS_NIKOLA,
                            MONS_AZRAEL, MONS_NESSOS, MONS_AGNES, MONS_AIZUL,
                            MONS_MAUD, MONS_LOUISE, MONS_NERGALLE, MONS_KIRKE, -1);
        break;
    case 5: // depth <= 19
        ret = random_choose(MONS_SNORG, MONS_LOUISE, MONS_FRANCES, MONS_KHUFU,
                            MONS_RUPERT, MONS_NORRIS, MONS_AGNES,
                            MONS_AZRAEL, MONS_NESSOS, MONS_NERGALLE,
                            MONS_ROXANNE, MONS_SAINT_ROKA, MONS_KIRKE,
                            MONS_WIGLAF, -1);
        break;
    case 6: // depth > 19
    default:
        ret = random_choose(MONS_FRANCES, MONS_MARA, MONS_WIGLAF, MONS_MENNAS,
                            MONS_XTAHUA, MONS_NORRIS, MONS_FREDERICK, MONS_TIAMAT,
                            MONS_MARGERY, MONS_BORIS, MONS_SAINT_ROKA, -1);
    }

    return ret;
}

static monster_type _pick_unique(int level)
{
    // Pick generic unique depending on depth.
    return (level <=  3) ? _choose_unique_by_depth(0) :
           (level <=  7) ? _choose_unique_by_depth(1) :
           (level <=  9) ? _choose_unique_by_depth(2) :
           (level <= 13) ? _choose_unique_by_depth(3) :
           (level <= 16) ? _choose_unique_by_depth(4) :
           (level <= 19) ? _choose_unique_by_depth(5) :
                           _choose_unique_by_depth(6);
}

// Ask for a location and place a trap there. Returns true
// for success.
bool create_trap(trap_type spec_type)
{
    dist abild;
    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.may_target_monster = false;
    args.top_prompt = "Make ";
    args.top_prompt += trap_name(spec_type);
    args.top_prompt += " trap where?";
    direction(abild, args);
    if (!abild.isValid)
    {
        if (abild.isCancel)
            canned_msg(MSG_OK);
        return false;
    }
    // only try to create on floor squares
    if (grd(abild.target) != DNGN_FLOOR)
    {
        mpr("You can't create a trap there!");
        return false;
    }
    bool result = place_specific_trap(abild.target, spec_type);

    if (result)
        grd(abild.target) = env.trap[env.tgrid(abild.target)].category();

    return result;
}

/**
 * Create an altar to the god of the player's choice.
 * @param wizmode if true, bypass some checks.
 */
bool zotdef_create_altar(bool wizmode)
{
    char specs[80];

    if (!wizmode && grd(you.pos()) != DNGN_FLOOR)
        return false;

    msgwin_get_line("Which god (by name)? ", specs, sizeof(specs));

    if (specs[0] == '\0')
        return false;

    string spec = lowercase_string(specs);

    god_type god = GOD_NO_GOD;

    for (int i = 1; i < NUM_GODS; ++i)
    {
        const god_type gi = static_cast<god_type>(i);

        if (!wizmode && is_unavailable_god(gi))
            continue;

        if (lowercase_string(god_name(gi)).find(spec) != string::npos)
        {
            god = gi;
            break;
        }
    }

    if (god == GOD_NO_GOD)
    {
        mpr("That god doesn't seem to be taking followers today.");
        return false;
    }
    else
    {
        dungeon_feature_type feat = altar_for_god(god);
        dungeon_terrain_changed(you.pos(), feat, false);

        if (wizmode)
            pray();
        else
            mprf("An altar to %s grows from the floor before you!",
                 god_name(god).c_str());

        return true;
    }
}

bool create_zotdef_ally(monster_type mtyp, const char *successmsg)
{
    if (count_allies() > MAX_MONSTERS / 2)
    {
        mpr("The place is too crowded already!");
        return false;
    }

    dist abild;
    string msg = "Make ";
    msg += get_monster_data(mtyp)->name;
    msg += " where?";

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.may_target_monster = false;
    args.top_prompt = msg;
    direction(abild, args);

    if (!abild.isValid)
    {
        if (abild.isCancel)
            canned_msg(MSG_OK);
        return false;
    }
    if (!mons_place(mgen_data(mtyp, BEH_FRIENDLY, &you, 0, 0, abild.target,
                   you.pet_target)))
    {
        mpr("You can't create it there!");
        return false;
    }
    mpr(successmsg);
    return true;
}

void zotdef_bosses_check()
{
    if ((you.num_turns + 1) % ZOTDEF_CYCLE_LENGTH == 0)
    {
        if (monster *mon = zotdef_spawn(true))        // boss monster=true
        {
            const char *msg = "You sense that a powerful threat has arrived.";
            if (!(((you.num_turns + 1) / ZOTDEF_CYCLE_LENGTH) % ZOTDEF_RUNE_FREQ))
            {
                const rune_type which_rune = _get_rune();
                int ip = items(1, OBJ_MISCELLANY, MISC_RUNE_OF_ZOT, true,
                               which_rune, which_rune);
                int *const item_made = &ip;
                if (*item_made != NON_ITEM && *item_made != -1)
                {
                    mitm[ip].plus = which_rune;
                    move_item_to_grid(item_made, mon->pos());
                    msg = "You feel a sense of great excitement!";
                }
            }
            _zotdef_danger_msg(msg);
        }

        // since you don't move between maps, any crash would be fatal
        if (!crawl_state.disables[DIS_SAVE_CHECKPOINTS])
            save_game(false);
    }

    if ((you.num_turns + 1) % ZOTDEF_CYCLE_LENGTH == ZOTDEF_CYCLE_INTERVAL)
    {
        // Set the next wave
        zotdef_set_wave();
    }
}

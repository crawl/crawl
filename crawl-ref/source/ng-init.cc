/*
 *  File:       ng-init.cc
 *  Summary:    Initializing non-player-related parts of a new game.
 *
 * TODO: 'you' shouldn't occur here.
 *       Some of these might fit better elsewhere.
 */

#include "AppHdr.h"

#include "branch.h"
#include "describe.h"
#include "dungeon.h"
#include "itemname.h"
#include "player.h"
#include "random.h"
#include "store.h"

static unsigned char _random_potion_description()
{
    int desc, nature, colour;

    do
    {
        desc = random2( PDQ_NQUALS * PDC_NCOLOURS );

        if (coinflip())
            desc %= PDC_NCOLOURS;

        nature = PQUAL(desc);
        colour = PCOLOUR(desc);

        // nature and colour correspond to primary and secondary in
        // itemname.cc.  This check ensures clear potions don't get odd
        // qualifiers.
    }
    while (colour == PDC_CLEAR && nature > PDQ_VISCOUS
           || desc == PDESCS(PDC_CLEAR));

    return static_cast<unsigned char>(desc);
}

// Determine starting depths of branches.
void initialise_branch_depths()
{
    branches[BRANCH_ECUMENICAL_TEMPLE].startdepth = random_range(4, 7);
    branches[BRANCH_ORCISH_MINES].startdepth      = random_range(6, 11);
    branches[BRANCH_ELVEN_HALLS].startdepth       = random_range(3, 4);
    branches[BRANCH_LAIR].startdepth              = random_range(8, 13);
    branches[BRANCH_HIVE].startdepth              = random_range(11, 16);
    branches[BRANCH_SLIME_PITS].startdepth        = random_range(5, 8);
    if ( coinflip() )
    {
        branches[BRANCH_SWAMP].startdepth  = random_range(2, 5);
        branches[BRANCH_SHOALS].startdepth = -1;
    }
    else
    {
        branches[BRANCH_SWAMP].startdepth  = -1;
        branches[BRANCH_SHOALS].startdepth = random_range(2, 5);
    }
    branches[BRANCH_SNAKE_PIT].startdepth      = random_range(3, 6);
    branches[BRANCH_VAULTS].startdepth         = random_range(14, 19);
    branches[BRANCH_CRYPT].startdepth          = random_range(2, 4);
    branches[BRANCH_HALL_OF_BLADES].startdepth = random_range(4, 6);
    branches[BRANCH_TOMB].startdepth           = random_range(2, 3);
}

#define MAX_OVERFLOW_LEVEL 9

// Determine which altars go into the Ecumenical Temple, which go into
// overflow temples, and on what level the overflow temples are.
void initialise_temples()
{
    std::vector<god_type> god_list;

    for (int i = 0; i < NUM_GODS; i++)
    {
        god_type god = (god_type) i;

        // These never appear in any temples.
        switch(god)
        {
        case GOD_NO_GOD:
        case GOD_LUGONU:
        case GOD_BEOGH:
        case GOD_JIYVA:
            continue;

        default:
            break;
        }

        god_list.push_back(god);
    }

    std::random_shuffle(god_list.begin(), god_list.end());

    std::vector<god_type> overflow_gods;

    while (god_list.size() > 12)
    {
        overflow_gods.push_back(god_list.back());
        god_list.pop_back();
    }

    CrawlVector &temple_gods
        = you.props[TEMPLE_GODS_KEY].new_vector(SV_BYTE);

    for (unsigned int i = 0; i < god_list.size(); i++)
        temple_gods.push_back( (char) god_list[i] );

    CrawlVector &overflow_temples
        = you.props[OVERFLOW_TEMPLES_KEY].new_vector(SV_VEC);
    overflow_temples.resize(MAX_OVERFLOW_LEVEL);

    // NOTE: The overflow temples don't have to contain only one
    // altar; they can contain any number of altars, so long as there's
    // at least one vault definition with the tag "overflow_temple_num"
    // (where "num" is the number of altars).
    for (unsigned int i = 0; i < overflow_gods.size(); i++)
    {
        const unsigned int level = random_range(2, MAX_OVERFLOW_LEVEL);

        // List of overflow temples on this level.
        CrawlVector &level_temples
            = overflow_temples[level - 1].get_vector();

        CrawlHashTable temple;

        CrawlVector &gods
            = temple[TEMPLE_GODS_KEY].new_vector(SV_BYTE);

        // Only single-altar overflow temples for now.
        gods.push_back( (char) overflow_gods[i] );

        level_temples.push_back(temple);
    }
}

static int _get_random_porridge_desc()
{
    return PDESCQ(PDQ_GLUGGY, one_chance_in(3) ? PDC_BROWN
                                               : PDC_WHITE);
}

static int _get_random_coagulated_blood_desc()
{
    potion_description_qualifier_type qualifier = PDQ_NONE;
    while (true)
    {
        switch (random2(4))
        {
        case 0:
            qualifier = PDQ_GLUGGY;
            break;
        case 1:
            qualifier = PDQ_LUMPY;
            break;
        case 2:
            qualifier = PDQ_SEDIMENTED;
            break;
        case 3:
            qualifier = PDQ_VISCOUS;
            break;
        }
        potion_description_colour_type colour = (coinflip() ? PDC_RED
                                                            : PDC_BROWN);

        int desc = PDESCQ(qualifier, colour);

        if (you.item_description[IDESC_POTIONS][POT_BLOOD] != desc)
            return desc;
    }
}

static int _get_random_blood_desc()
{
    return PDESCQ(coinflip() ? PDQ_NONE :
                  coinflip() ? PDQ_VISCOUS
                             : PDQ_SEDIMENTED, PDC_RED);
}

void initialise_item_descriptions()
{
    // Must remember to check for already existing colours/combinations.
    you.item_description.init(255);

    you.item_description[IDESC_POTIONS][POT_WATER] = PDESCS(PDC_CLEAR);
    you.item_description[IDESC_POTIONS][POT_PORRIDGE]
        = _get_random_porridge_desc();
    you.item_description[IDESC_POTIONS][POT_BLOOD]
        = _get_random_blood_desc();
    you.item_description[IDESC_POTIONS][POT_BLOOD_COAGULATED]
        = _get_random_coagulated_blood_desc();

    // The order here must match that of IDESC in describe.h
    const int max_item_number[6] = { NUM_WANDS,
                                     NUM_POTIONS,
                                     NUM_SCROLLS,
                                     NUM_JEWELLERY,
                                     NUM_SCROLLS,
                                     NUM_STAVES };

    for (int i = 0; i < NUM_IDESC; i++)
    {
        // Only loop until NUM_WANDS etc.
        for (int j = 0; j < max_item_number[i]; j++)
        {
            // Don't override predefines
            if (you.item_description[i][j] != 255)
                continue;

            // Pick a new description until it's good.
            while (true)
            {
                // The numbers below are always secondary * primary,
                // except for scrolls. (See itemname.cc.)
                switch (i)
                {
                case IDESC_WANDS: // wands
                    you.item_description[i][j] = random2( 16 * 12 );
                    if (coinflip())
                        you.item_description[i][j] %= 12;
                    break;

                case IDESC_POTIONS: // potions
                    you.item_description[i][j] = _random_potion_description();
                    break;

                case IDESC_SCROLLS: // scrolls: random seed for the name
                case IDESC_SCROLLS_II:
                    you.item_description[i][j] = random2(151);
                    break;

                case IDESC_RINGS: // rings
                    you.item_description[i][j] = random2( 13 * 13 );
                    if (coinflip())
                        you.item_description[i][j] %= 13;
                    break;

                case IDESC_STAVES: // staves and rods
                    you.item_description[i][j] = random2( 10 * 4 );
                    break;
                }

                bool is_ok = true;

                // Test whether we've used this description before.
                // Don't have p < j because some are preassigned.
                for (int p = 0; p < max_item_number[i]; p++)
                {
                    if (p == j)
                        continue;

                    if (you.item_description[i][p] == you.item_description[i][j])
                    {
                        is_ok = false;
                        break;
                    }
                }
                if (is_ok)
                    break;
            }
        }
    }
}

void fix_up_jiyva_name()
{
    do
        you.second_god_name = make_name(random_int(), false, 8, 'J');
    while (strncmp(you.second_god_name.c_str(), "J", 1) != 0);

    you.second_god_name = replace_all(you.second_god_name, " ", "");
}

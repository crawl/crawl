/**
 * @file
 * @brief Initializing non-player-related parts of a new game.
**/
/* TODO: 'you' shouldn't occur here.
 *       Some of these might fit better elsewhere.
 */

#include "AppHdr.h"

#include "ng-init.h"

#include "branch.h"
#include "describe.h"
#include "dungeon.h"
#include "end.h"
#include "itemname.h"
#include "libutil.h"
#include "maps.h"
#include "random-weight.h"
#include "religion.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "unicode.h"

#ifdef DEBUG_DIAGNOSTICS
#define DEBUG_TEMPLES
#endif

static uint8_t _random_potion_description()
{
    int desc;

    desc = random2(PDQ_NQUALS * PDC_NCOLOURS);

    if (coinflip())
        desc %= PDC_NCOLOURS;

    // nature and colour correspond to primary and secondary in
    // itemname.cc.

#if TAG_MAJOR_VERSION == 34
    if (PCOLOUR(desc) == PDC_CLEAR) // only water can be clear, re-roll
        return _random_potion_description();
#endif

    return desc;
}

// Determine starting depths of branches.
void initialise_branch_depths()
{
    root_branch = BRANCH_DUNGEON;

    // XXX: Should this go elsewhere?
    branch_bribe.init(0);

    for (branch_iterator it; it; ++it)
        brentry[it->id].clear();

    if (crawl_state.game_is_sprint())
    {
        brdepth.init(-1);
        brdepth[BRANCH_DUNGEON] = 1;
        return;
    }

    if (crawl_state.game_is_zotdef())
    {
        root_branch = BRANCH_ZOT;
        brdepth.init(-1);
        brdepth[BRANCH_ZOT] = 1;
        brdepth[BRANCH_BAZAAR] = 1;
        return;
    }

    for (int branch = 0; branch < NUM_BRANCHES; ++branch)
    {
        const Branch *b = &branches[branch];
        ASSERT(b->id == branch);
    }

    for (branch_iterator it; it; ++it)
    {
        if (!branch_is_unfinished(it->id) && it->parent_branch != NUM_BRANCHES)
        {
            brentry[it->id] = level_id(it->parent_branch,
                                       random_range(it->mindepth,
                                                    it->maxdepth));
        }
    }

    // You will get one of Shoals/Swamp and one of Spider/Snake.
    // This way you get one "water" branch and one "poison" branch.
    branch_type disabled_branch[] =
    {
        random_choose(BRANCH_SWAMP, BRANCH_SHOALS, -1),
        random_choose(BRANCH_SNAKE, BRANCH_SPIDER, -1),
    };

    for (unsigned int i = 0; i < ARRAYSZ(disabled_branch); ++i)
    {
        dprf("Disabling branch: %s", branches[disabled_branch[i]].shortname);
        brentry[disabled_branch[i]].clear();
    }

    for (branch_iterator it; it; ++it)
        brdepth[it->id] = it->numlevels;
}

#define MAX_OVERFLOW_LEVEL 9

static void _use_overflow_temple(vector<god_type> temple_gods)
{
    CrawlVector &overflow_temples
        = you.props[OVERFLOW_TEMPLES_KEY].get_vector();

    const unsigned int level = random_range(2, MAX_OVERFLOW_LEVEL);

    // List of overflow temples on this level.
    CrawlVector &level_temples = overflow_temples[level - 1].get_vector();

    CrawlHashTable temple;

    CrawlVector &gods = temple[TEMPLE_GODS_KEY].new_vector(SV_BYTE);

    for (unsigned int i = 0; i < temple_gods.size(); i++)
        gods.push_back((char) temple_gods[i]);

    level_temples.push_back(temple);
}

// Determine which altars go into the Ecumenical Temple, which go into
// overflow temples, and on what level the overflow temples are.
void initialise_temples()
{
    //////////////////////////////////////////
    // First determine main temple map to use.
    level_id ecumenical(BRANCH_TEMPLE, 1);

    map_def *main_temple = NULL;
    for (int i = 0; i < 10; i++)
    {
        int altar_count = 0;

        main_temple
            = const_cast<map_def*>(random_map_for_place(ecumenical, false));

        if (main_temple == NULL)
            end(1, false, "No temples?!");

        if (main_temple->has_tag("temple_variable"))
        {
            vector<int> sizes;
            vector<string> tag_list = main_temple->get_tags();
            for (unsigned int j = 0; j < tag_list.size(); j++)
            {
                if (starts_with(tag_list[j], "temple_altars_"))
                {
                    sizes.push_back(
                        atoi(
                            strip_tag_prefix(
                                tag_list[j], "temple_altars_").c_str()));
                }
            }
            if (sizes.empty())
            {
                mprf(MSGCH_ERROR,
                     "Temple %s set as variable but has no sizes.",
                     main_temple->name.c_str());
                main_temple = NULL;
                continue;
            }
            altar_count =
                you.props[TEMPLE_SIZE_KEY].get_int() =
                    sizes[random2(sizes.size())];
        }

        dgn_map_parameters mp(make_stringf("temple_altars_%d", altar_count));

        // Without all this find_glyph() returns 0.
        string err;
              main_temple->load();
              main_temple->reinit();
        err = main_temple->run_lua(true);

        if (!err.empty())
        {
            mprf(MSGCH_ERROR, "Temple %s: %s", main_temple->name.c_str(),
                 err.c_str());
            main_temple = NULL;
            you.props.erase(TEMPLE_SIZE_KEY);
            continue;
        }

              main_temple->fixup();
        err = main_temple->resolve();

        if (!err.empty())
        {
            mprf(MSGCH_ERROR, "Temple %s: %s", main_temple->name.c_str(),
                 err.c_str());
            main_temple = NULL;
            you.props.erase(TEMPLE_SIZE_KEY);
            continue;
        }
        break;
    }

    if (main_temple == NULL)
        end(1, false, "No valid temples.");

    you.props[TEMPLE_MAP_KEY] = main_temple->name;

    const vector<coord_def> altar_coords
        = main_temple->find_glyph('B');
    const unsigned int main_temple_size = altar_coords.size();

    if (main_temple_size == 0)
    {
        end(1, false, "Main temple '%s' has no altars",
            main_temple->name.c_str());
    }

#ifdef DEBUG_TEMPLES
    mprf(MSGCH_DIAGNOSTICS, "Chose main temple %s, size %u",
         main_temple->name.c_str(), main_temple_size);
#endif

    ///////////////////////////////////
    // Now set up the overflow temples.

    vector<god_type> god_list = temple_god_list();
    shuffle_array(god_list);

    vector<god_type> overflow_gods;

    while (god_list.size() > main_temple_size)
    {
        overflow_gods.push_back(god_list.back());
        god_list.pop_back();
    }

#ifdef DEBUG_TEMPLES
    mprf(MSGCH_DIAGNOSTICS, "%u overflow altars", (unsigned int)overflow_gods.size());
#endif

    CrawlVector &temple_gods
        = you.props[TEMPLE_GODS_KEY].new_vector(SV_BYTE);

    for (unsigned int i = 0; i < god_list.size(); i++)
        temple_gods.push_back((char) god_list[i]);

    CrawlVector &overflow_temples
        = you.props[OVERFLOW_TEMPLES_KEY].new_vector(SV_VEC);
    overflow_temples.resize(MAX_OVERFLOW_LEVEL);

    // Count god overflow temple weights.
    int overflow_weights[NUM_GODS + 1];
    overflow_weights[0] = 0;

    for (unsigned int i = 1; i < NUM_GODS; i++)
    {
        string mapname = make_stringf("temple_overflow_generic_%d", i);
        mapref_vector maps = find_maps_for_tag(mapname);
        if (!maps.empty())
        {
            int chance = 0;
            for (auto map : maps)
            {
                // XXX: this should handle level depth better
                chance += map->weight(level_id(BRANCH_DUNGEON,
                                               MAX_OVERFLOW_LEVEL));
            }
            overflow_weights[i] = chance;
        }
        else
            overflow_weights[i] = 0;
    }

    // Try to find combinations of overflow gods that have specialised
    // overflow vaults.
multi_overflow:
    for (unsigned int i = 1, size = 1 << overflow_gods.size();
         i <= size; i++)
    {
        unsigned int num = count_bits(i);

        // TODO: possibly make this place single-god vaults too?
        // XXX: upper limit on num here because this code gets really
        // slow otherwise.
        if (num <= 1 || num > 3)
            continue;

        vector<god_type> this_temple_gods;
        vector<god_type> new_overflow_gods;

        string tags = make_stringf("temple_overflow_%d", num);
        for (unsigned int j = 0; j < overflow_gods.size(); j++)
        {
            if (i & (1 << j))
            {
                string name = replace_all(god_name(overflow_gods[j]), " ", "_");
                lowercase(name);
                tags = tags + " temple_overflow_" + name;
                this_temple_gods.push_back(overflow_gods[j]);
            }
            else
                new_overflow_gods.push_back(overflow_gods[j]);
        }

        mapref_vector maps = find_maps_for_tag(tags);
        if (maps.empty())
            continue;

        if (overflow_weights[num] > 0)
        {
            int chance = 0;
            for (auto map : maps)
            {
                chance += map->weight(level_id(BRANCH_DUNGEON,
                                               MAX_OVERFLOW_LEVEL));
            }
            if (!x_chance_in_y(chance, overflow_weights[num] + chance))
                continue;
        }

        _use_overflow_temple(this_temple_gods);

        overflow_gods = new_overflow_gods;

        goto multi_overflow;
    }

    // NOTE: The overflow temples don't have to contain only one
    // altar; they can contain any number of altars, so long as there's
    // at least one vault definition with the tag "overflow_temple_num"
    // (where "num" is the number of altars).
    for (unsigned int i = 0, size = overflow_gods.size(); i < size; i++)
    {
        unsigned int remaining_size = size - i;
        // At least one god.
        vector<god_type> this_temple_gods;
        this_temple_gods.push_back(overflow_gods[i]);

        // Maybe place a larger overflow temple.
        if (remaining_size > 1 && one_chance_in(remaining_size + 1))
        {
            vector<pair<unsigned int, int> > num_weights;
            unsigned int num_gods = 1;

            // Randomly choose from the sizes which have maps.
            for (unsigned int j = 2; j <= remaining_size; j++)
            {
                if (overflow_weights[j] > 0)
                {
                    num_weights.push_back(
                        pair<unsigned int, int>(j, overflow_weights[j]));
                }
            }
            if (!num_weights.empty())
                num_gods = *(random_choose_weighted(num_weights));

            // Add any extra gods (the first was added already).
            for (; num_gods > 1; i++, num_gods--)
                this_temple_gods.push_back(overflow_gods[i + 1]);
        }

        _use_overflow_temple(this_temple_gods);
    }
}

#if TAG_MAJOR_VERSION == 34
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

        uint32_t desc = PDESCQ(qualifier, colour);

        if (you.item_description[IDESC_POTIONS][POT_BLOOD] != desc)
            return desc;
    }
}
#endif

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

    you.item_description[IDESC_POTIONS][POT_BLOOD]
        = _get_random_blood_desc();
#if TAG_MAJOR_VERSION == 34
    you.item_description[IDESC_POTIONS][POT_BLOOD_COAGULATED]
        = _get_random_coagulated_blood_desc();
    you.item_description[IDESC_POTIONS][POT_PORRIDGE]
        = _get_random_porridge_desc();
#endif

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
                switch (i)
                {
                case IDESC_WANDS: // wands
                    you.item_description[i][j] = random2(NDSC_WAND_PRI
                                                         * NDSC_WAND_SEC);
                    if (coinflip())
                        you.item_description[i][j] %= NDSC_WAND_PRI;
                    break;

                case IDESC_POTIONS: // potions
                    you.item_description[i][j] = _random_potion_description();
                    break;


#if TAG_MAJOR_VERSION == 34
                case IDESC_SCROLLS_II: // unused but validated
#endif
                case IDESC_SCROLLS: // scrolls: random seed for the name
                {
                    // this is very weird and probably a linleyism.
                    const int seed_1 = random2(151); // why 151?
                    const int seed_2 = random2(151);
                    const int seed_3 = OBJ_SCROLLS; // yes, really
                    you.item_description[i][j] =   seed_1
                                                | (seed_2 << 8)
                                                | (seed_3 << 16);
                    break;
                }

                case IDESC_RINGS: // rings and amulets
                    you.item_description[i][j] = random2(NDSC_JEWEL_PRI
                                                         * NDSC_JEWEL_SEC);
                    if (coinflip())
                        you.item_description[i][j] %= NDSC_JEWEL_PRI;
                    break;

                case IDESC_STAVES: // staves and rods
                    you.item_description[i][j] = random2(NDSC_STAVE_PRI
                                                         * NDSC_STAVE_SEC);
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
    {
        you.jiyva_second_name = make_name(random_int(), false, 8, 'J');
    }
    while (strncmp(you.jiyva_second_name.c_str(), "J", 1) != 0);

    you.jiyva_second_name = replace_all(you.jiyva_second_name, " ", "");
}

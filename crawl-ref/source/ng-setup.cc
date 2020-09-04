#include "AppHdr.h"

#include "ng-setup.h"

#include "adjust.h"
#include "dungeon.h"
#include "end.h"
#include "files.h"
#include "god-companions.h"
#include "hints.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "jobs.h"
#include "message.h"
#include "mutation.h"
#include "ng-init.h"
#include "ng-wanderer.h"
#include "options.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"

#define MIN_START_STAT       3

static void _init_player()
{
    you = player();
    dlua.callfn("dgn_clear_data", "");
}

// Make sure no stats are unacceptably low
// (currently possible only for GhBe - 1KB)
static void _unfocus_stats()
{
    int needed;

    for (int i = 0; i < NUM_STATS; ++i)
    {
        int j = (i + 1) % NUM_STATS;
        int k = (i + 2) % NUM_STATS;
        if ((needed = MIN_START_STAT - you.base_stats[i]) > 0)
        {
            if (you.base_stats[j] > you.base_stats[k])
                you.base_stats[j] -= needed;
            else
                you.base_stats[k] -= needed;
            you.base_stats[i] = MIN_START_STAT;
        }
    }
}

// Some consumables to make the starts of Sprint a little easier.
static void _give_bonus_items()
{
    newgame_make_item(OBJ_POTIONS, POT_CURING);
    newgame_make_item(OBJ_POTIONS, POT_HEAL_WOUNDS);
    newgame_make_item(OBJ_POTIONS, POT_HASTE);
    newgame_make_item(OBJ_POTIONS, POT_MAGIC, 2);
    newgame_make_item(OBJ_POTIONS, POT_BERSERK_RAGE);
    newgame_make_item(OBJ_SCROLLS, SCR_BLINKING);
}

static void _autopickup_ammo(missile_type missile)
{
    if (Options.autopickup_starting_ammo)
        you.force_autopickup[OBJ_MISSILES][missile] = AP_FORCE_ON;
}

/**
 * Make an item during character creation.
 *
 * Puts the item in the first available slot in the inventory, equipping or
 * memorising the first spell from it as appropriate. If the item would be
 * useless, we try with a possibly more useful sub_type, then, if it's still
 * useless, give up and don't create any item.
 *
 * @param base, sub_type what the item is
 * @param qty            what size stack to make
 * @param plus           what the value of item_def::plus should be
 * @param force_ego      what the value of item_def::special should be
 * @param force_tutorial whether to create it even in the tutorial
 * @returns a pointer to the item created, if any.
 */
item_def* newgame_make_item(object_class_type base,
                            int sub_type, int qty, int plus,
                            int force_ego, bool force_tutorial)
{
    // Don't set normal equipment in the tutorial.
    if (!force_tutorial && crawl_state.game_is_tutorial())
        return nullptr;

    // not an actual item
    // the WPN_UNKNOWN case is used when generating a paper doll during
    // character creation
    if (sub_type == WPN_UNARMED || sub_type == WPN_UNKNOWN)
        return nullptr;

    int slot;
    for (slot = 0; slot < ENDOFPACK; ++slot)
    {
        item_def& item = you.inv[slot];
        if (!item.defined())
            break;

        if (item.is_type(base, sub_type) && item.brand == force_ego
            && is_stackable_item(item))
        {
            item.quantity += qty;
            return &item;
        }
    }

    item_def &item(you.inv[slot]);
    item.base_type = base;
    item.sub_type  = sub_type;
    item.quantity  = qty;
    item.plus      = plus;
    item.brand     = force_ego;

    // If the character is restricted in wearing the requested armour,
    // hand out a replacement instead.
    if (item.base_type == OBJ_ARMOUR
        && !can_wear_armour(item, false, false))
    {
        if (item.sub_type == ARM_HELMET || item.sub_type == ARM_HAT)
            item.sub_type = ARM_HAT;
        else if (item.sub_type == ARM_BUCKLER)
            item.sub_type = ARM_KITE_SHIELD;
        else if (is_shield(item))
            item.sub_type = ARM_BUCKLER;
        else
            item.sub_type = ARM_ROBE;
    }

    // Make sure we didn't get a stack of shields or such nonsense.
    ASSERT(item.quantity == 1 || is_stackable_item(item));

    // If that didn't help, nothing will.
    if (is_useless_item(item, false, true))
    {
        item = item_def();
        return nullptr;
    }

    if ((item.base_type == OBJ_WEAPONS && can_wield(&item, false, false)
        || item.base_type == OBJ_ARMOUR && can_wear_armour(item, false, false))
        && you.equip[get_item_slot(item)] == -1)
    {
        you.equip[get_item_slot(item)] = slot;
    }

    if (item.base_type == OBJ_MISSILES)
        _autopickup_ammo(static_cast<missile_type>(item.sub_type));
    // You can get the books without the corresponding items as a wanderer.
    else if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_GEOMANCY)
        _autopickup_ammo(MI_STONE);
    // You probably want to pick up both.
    if (item.is_type(OBJ_MISSILES, MI_SLING_BULLET))
        _autopickup_ammo(MI_STONE);

    origin_set_startequip(item);

    // Wanderers may or may not already have a spell. - bwr
    // Also, when this function gets called their possible randbook
    // has not been initialised and will trigger an ASSERT.
    if (item.base_type == OBJ_BOOKS && you.char_class != JOB_WANDERER)
    {
        spell_type which_spell = spells_in_book(item)[0];
        if (!spell_is_useless(which_spell, false, true)
            && spell_difficulty(which_spell) <= 1)
        {
            add_spell_to_memory(which_spell);
        }
    }

    return &item;
}

static void _give_ranged_weapon(weapon_type weapon, int plus)
{
    ASSERT(weapon != NUM_WEAPONS);

    switch (weapon)
    {
    case WPN_SHORTBOW:
    case WPN_HAND_CROSSBOW:
    case WPN_HUNTING_SLING:
        newgame_make_item(OBJ_WEAPONS, weapon, 1, plus);
        break;
    default:
        break;
    }
}

static void _give_ammo(weapon_type weapon, int plus)
{
    ASSERT(weapon != NUM_WEAPONS);

    switch (weapon)
    {
    case WPN_THROWN:
        if (species_can_throw_large_rocks(you.species))
            newgame_make_item(OBJ_MISSILES, MI_LARGE_ROCK, 4 + plus);
        else if (you.body_size(PSIZE_TORSO) <= SIZE_SMALL)
            newgame_make_item(OBJ_MISSILES, MI_BOOMERANG, 8 + 2 * plus);
        else
            newgame_make_item(OBJ_MISSILES, MI_JAVELIN, 5 + plus);
        newgame_make_item(OBJ_MISSILES, MI_THROWING_NET, 2);
        break;
    case WPN_SHORTBOW:
        newgame_make_item(OBJ_MISSILES, MI_ARROW, 20);
        break;
    case WPN_HAND_CROSSBOW:
        newgame_make_item(OBJ_MISSILES, MI_BOLT, 20);
        break;
    case WPN_HUNTING_SLING:
        newgame_make_item(OBJ_MISSILES, MI_SLING_BULLET, 20);
        break;
    default:
        break;
    }
}

void give_items_skills(const newgame_def& ng)
{
    create_wanderer();

    switch (you.char_class)
    {
    case JOB_BERSERKER:
        you.religion = GOD_TROG;
        you.piety = 35;

        if (you_can_wear(EQ_BODY_ARMOUR))
            you.skills[SK_ARMOUR] += 2;
        else
        {
            you.skills[SK_DODGING]++;
            if (!is_useless_skill(SK_ARMOUR))
                you.skills[SK_ARMOUR]++; // converted later
        }
        break;

    case JOB_ARTIFICER:
    {
        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;
    }
    case JOB_CHAOS_KNIGHT:
    {
        you.religion = GOD_XOM;
        you.piety = 100;
        int timeout_rnd = random2(40);
        timeout_rnd += random2(40); // force a sequence point between random2s
        you.gift_timeout = max(5, timeout_rnd);

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;
    }
    case JOB_ABYSSAL_KNIGHT:
        you.religion = GOD_LUGONU;
        if (!crawl_state.game_is_sprint())
            you.chapter = CHAPTER_POCKET_ABYSS;
        you.piety = 38;

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;

        break;

    default:
        break;
    }

    if (you.char_class == JOB_ABYSSAL_KNIGHT)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, +1);
    else if (you.char_class == JOB_CHAOS_KNIGHT)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, 0, SPWPN_CHAOS);
    else if (job_gets_ranged_weapons(you.char_class))
        _give_ranged_weapon(ng.weapon, you.char_class == JOB_HUNTER ? 1 : 0);
    else if (job_has_weapon_choice(you.char_class))
        newgame_make_item(OBJ_WEAPONS, ng.weapon);

    give_job_equipment(you.char_class);
    give_job_skills(you.char_class);

    if (job_gets_ranged_weapons(you.char_class))
        _give_ammo(ng.weapon, you.char_class == JOB_HUNTER ? 1 : 0);

    if (you.species == SP_FELID)
    {
        you.skills[SK_THROWING] = 0;
        you.skills[SK_SHIELDS] = 0;
    }

    if (!you_worship(GOD_NO_GOD))
    {
        you.worshipped[you.religion] = 1;
        set_god_ability_slots();
        if (!you_worship(GOD_XOM))
            you.piety_max[you.religion] = you.piety;
    }
}

static void _setup_tutorial_miscs()
{
    // Allow for a few specific hint mode messages.
    // A few more will be initialised by the tutorial map.
    tutorial_init_hints();

    // No gold to begin with.
    you.gold = 0;

    // Give them some mana to play around with.
    you.mp_max_adj += 2;

    newgame_make_item(OBJ_ARMOUR, ARM_ROBE, 1, 0, 0, true);

    // No need for Shields skill without shield.
    you.skills[SK_SHIELDS] = 0;

    // Some spellcasting for the magic tutorial.
    if (crawl_state.map.find("tutorial_lesson4") != string::npos)
        you.skills[SK_SPELLCASTING] = 1;
}

static void _give_basic_knowledge()
{
    identify_inventory();

    // Removed item types are handled in _set_removed_types_as_identified.
}

static void _setup_generic(const newgame_def& ng,
                          bool normal_dungeon_setup /*for catch2-tests*/);

// Initialise a game based on the choice stored in ng.
void setup_game(const newgame_def& ng,
                bool normal_dungeon_setup /*for catch2-tests */)
{
    crawl_state.type = ng.type; // by default
    if (Options.seed_from_rc && ng.type != GAME_TYPE_CUSTOM_SEED)
    {
        // rc seed overrides seed from other sources, and forces a normal game
        // to be a custom seed game. There is currently no special designation
        // for sprint etc. games that involve a custom seed.
        // TODO: does this make any sense for hints mode?
        Options.seed = Options.seed_from_rc;
        if (ng.type == GAME_TYPE_NORMAL)
            crawl_state.type = GAME_TYPE_CUSTOM_SEED;
    }
    else if (Options.seed && ng.type != GAME_TYPE_CUSTOM_SEED)
    {
        // there's a seed lingering in the options, but we shouldn't use it.
        Options.seed = 0;
    }
    else if (!Options.seed && ng.type == GAME_TYPE_CUSTOM_SEED)
        crawl_state.type = GAME_TYPE_NORMAL;

    crawl_state.map  = ng.map;

    switch (crawl_state.type)
    {
    case GAME_TYPE_NORMAL:
    case GAME_TYPE_CUSTOM_SEED:
    case GAME_TYPE_TUTORIAL:
    case GAME_TYPE_SPRINT:
        break;
    case GAME_TYPE_HINTS:
        init_hints();
        break;
    case GAME_TYPE_ARENA:
    default:
        die("Bad game type");
        end(-1);
    }

    _setup_generic(ng, normal_dungeon_setup);
}

static void _free_up_slot(char letter)
{
    for (int slot = 0; slot < ENDOFPACK; ++slot)
    {
        if (!you.inv[slot].defined())
        {
            swap_inv_slots(letter_to_index(letter),
                           slot, false);
            break;
        }
    }
}

void initial_dungeon_setup()
{
    rng::generator levelgen_rng(BRANCH_DUNGEON);

    initialise_branch_depths();
    initialise_temples();
    init_level_connectivity();
    initialise_item_descriptions();
}

static void _setup_generic(const newgame_def& ng,
                           bool normal_dungeon_setup /*for catch2-tests*/)
{
    // this seems non-ideal, but messages are not being displayed at this point
    // so if a force_more_message triggers, the more will just show on a blank
    // screen. TODO: it's not clear to me why the more can show up without
    // the message that triggered it.
    unwind_bool no_more(crawl_state.show_more_prompt, false);

    rng::reset(); // initialize rng from Options.seed
    _init_player();
    you.game_seed = crawl_state.seed;
    you.deterministic_levelgen = Options.incremental_pregen;

#if TAG_MAJOR_VERSION == 34
    // Avoid the remove_dead_shops() Gozag fixup in new games: see
    // ShoppingList::item_type_identified().
    you.props[REMOVED_DEAD_SHOPS_KEY] = true;
#endif

    // Needs to happen before we give the player items, so that it's safe to
    // check whether those items need to be removed from their shopping list.
    shopping_list.refresh();

    you.your_name  = ng.name;
    you.species    = ng.species;
    you.char_class = ng.job;

    you.chr_class_name = get_job_name(you.char_class);

    species_stat_init(you.species);     // must be down here {dlb}

    // Before we get into the inventory init, set light radius based
    // on species vision.
    update_vision_range();

    job_stat_init(you.char_class);

    _unfocus_stats();

    give_basic_mutations(you.species);

    // This function depends on stats and mutations being finalised.
    give_items_skills(ng);

    roll_demonspawn_mutations();

    if (crawl_state.game_is_sprint())
        _give_bonus_items();

    // Leave the a/b slots open so if the first thing you pick up is a weapon,
    // you can use ' to swap between your items.
    if (you.char_class == JOB_EARTH_ELEMENTALIST)
        _free_up_slot('a');
    if (you.char_class == JOB_ARCANE_MARKSMAN && ng.weapon != WPN_THROWN)
        _free_up_slot('b');

    // Give tutorial skills etc
    if (crawl_state.game_is_tutorial())
        _setup_tutorial_miscs();

    _give_basic_knowledge();

    {
        msg::suppress quiet;
        // Must be after _give_basic_knowledge
        add_held_books_to_library();
    }

    if (you.char_class == JOB_WANDERER)
        memorise_wanderer_spell();

    // A first pass to link the items properly.
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        auto &item = you.inv[i];
        if (!item.defined())
            continue;
        item.pos = ITEM_IN_INVENTORY;
        item.link = i;
        item.slot = index_to_letter(item.link);
        item_colour(item);  // set correct special and colour
    }

    if (you.equip[EQ_WEAPON] > 0)
        swap_inv_slots(0, you.equip[EQ_WEAPON], false);

    // A second pass to apply the item_slot option.
    for (auto &item : you.inv)
    {
        if (!item.defined())
            continue;
        if (!item.props.exists("adjusted"))
        {
            item.props["adjusted"] = true;
            auto_assign_item_slot(item);
        }
    }

    reassess_starting_skills();
    init_skill_order();
    init_can_currently_train();
    init_train();
    init_training();

    // Apply autoinscribe rules to inventory.
    request_autoinscribe();
    autoinscribe();

    // We calculate hp and mp here; all relevant factors should be
    // finalised by now. (GDL)
    calc_hp();
    calc_mp();

    // Make sure the starting player is fully charged up.
    set_hp(you.hp_max);
    set_mp(you.max_magic_points);

    if (normal_dungeon_setup)
        initial_dungeon_setup();

    // Generate the second name of Jiyva
    fix_up_jiyva_name();

    // Get rid of god companions left from previous games
    init_companions();

    // Create the save file.
    if (Options.no_save)
        you.save = new package();
    else
        you.save = new package(get_savedir_filename(you.your_name).c_str(),
                               true, true);
}

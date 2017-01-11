#include "AppHdr.h"

#include "ng-setup.h"

#include "ability.h"
#include "adjust.h"
#include "artefact.h"
#include "art-enum.h"
#include "decks.h"
#include "dungeon.h"
#include "end.h"
#include "files.h"
#include "food.h"
#include "god-companions.h"
#include "hints.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "jobs.h"
#include "mutation.h"
#include "ng-init.h"
#include "ng-wanderer.h"
#include "options.h"
#include "prompt.h"
#include "religion.h"
#include "tiledef-player.h"
#if TAG_MAJOR_VERSION == 34
# include "shopping.h" // REMOVED_DEAD_SHOPS_KEY
#endif
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
        you.force_autopickup[OBJ_MISSILES][missile] = 1;
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
    if (sub_type == WPN_UNARMED)
        return nullptr;

    int slot;
    for (slot = 0; slot < ENDOFPACK; ++slot)
    {
        if (base == OBJ_FOOD && slot == letter_to_index('e'))
            continue;

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
            item.sub_type = ARM_SHIELD;
        else if (is_shield(item))
            item.sub_type = ARM_BUCKLER;
        else
            item.sub_type = ARM_ROBE;
    }

    // Make sure we didn't get a stack of shields or such nonsense.
    ASSERT(item.quantity == 1 || is_stackable_item(item));

    // If that didn't help, nothing will.
    if (is_useless_item(item))
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
    else if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_CHANGES)
        _autopickup_ammo(MI_ARROW);
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
            newgame_make_item(OBJ_MISSILES, MI_TOMAHAWK, 8 + 2 * plus);
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

static const uint8_t _archa_num_unrands = 86;
static const unrand_type _archa_unrands[_archa_num_unrands]
{
    UNRAND_SINGING_SWORD,               // Singing Sword
    UNRAND_TROG,                        // Wrath of Trog
    UNRAND_VARIABILITY,                 // mace of Variability
    UNRAND_PRUNE,                       // glaive of Prune
    UNRAND_POWER,                       // sword of Power
    UNRAND_OLGREB,                      // staff of Olgreb
    UNRAND_WUCAD_MU,                    // staff of Wucad Mu
    UNRAND_VAMPIRES_TOOTH,              // Vampire's Tooth
    UNRAND_CURSES,                      // scythe of Curses
    UNRAND_TORMENT,                     // sceptre of Torment
    UNRAND_ZONGULDROK,                  // sword of Zonguldrok
    UNRAND_FAERIE,                      // faerie dragon scales
    UNRAND_BLOODBANE,                   // demon blade "Bloodbane"
    UNRAND_FLAMING_DEATH,               // scimitar of Flaming Death
    UNRAND_LEECH,                       // demon blade "Leech"
    UNRAND_CHILLY_DEATH,                // dagger of Chilly Death
    UNRAND_MORG,                        // dagger "Morg"
    UNRAND_FINISHER,                    // scythe "Finisher"
    UNRAND_PUNK,                        // sling "Punk"
    UNRAND_KRISHNA,                     // bow of Krishna "Sharnga"
    UNRAND_SKULLCRUSHER,                // giant club "Skullcrusher"
    UNRAND_GUARD,                       // glaive of the Guard
    UNRAND_JIHAD,                       // sword of Jihad
    UNRAND_DAMNATION,                   // arbalest "Damnation"
    UNRAND_DOOM_KNIGHT,                 // sword of the Doom Knight
    UNRAND_EOS,                         // morningstar "Eos"
    UNRAND_BOTONO,                      // spear of the Botono
    UNRAND_OCTOPUS_KING,                // trident of the Octopus King
    UNRAND_ARGA,                        // mithril axe "Arga"
    UNRAND_ELEMENTAL_STAFF,             // Elemental Staff
    UNRAND_SNIPER,                      // heavy crossbow "Sniper"
    UNRAND_PIERCER,                     // longbow "Piercer"
    UNRAND_WYRMBANE,                    // lance "Wyrmbane"
    UNRAND_SPRIGGANS_KNIFE,             // Spriggan's Knife
    UNRAND_PLUTONIUM_SWORD,             // plutonium sword
    UNRAND_UNDEADHUNTER,                // great mace "Undeadhunter"
    UNRAND_SNAKEBITE,                   // whip "Snakebite"
    UNRAND_CAPTAIN,                     // captain's cutlass
    UNRAND_STORM_BOW,                   // storm bow
    UNRAND_IGNORANCE,                   // large shield of Ignorance
    UNRAND_AUGMENTATION,                // robe of Augmentation
    UNRAND_THIEF,                       // cloak of the Thief
    UNRAND_DYROVEPREVA,                 // crown of Dyrovepreva
    UNRAND_BEAR_SPIRIT,                 // hat of the Bear Spirit
    UNRAND_MISFORTUNE,                  // robe of Misfortune
    UNRAND_BOOTS_ASSASSIN,              // boots of the Assassin
    UNRAND_LEAR,                        // Lear's hauberk
    UNRAND_ZHOR,                        // skin of Zhor
    UNRAND_SALAMANDER,                  // salamander hide armour
    UNRAND_WAR,                         // gauntlets of War
    UNRAND_RESISTANCE,                  // shield of Resistance
    UNRAND_FOLLY,                       // robe of Folly
    UNRAND_MAXWELL,                     // Maxwell's patent armour
    UNRAND_DRAGONMASK,                  // mask of the Dragon
    UNRAND_NIGHT,                       // robe of Night
    UNRAND_DRAGON_KING,                 // armour of the Dragon King
    UNRAND_ALCHEMIST,                   // hat of the Alchemist
    UNRAND_FENCERS,                     // fencer's gloves
    UNRAND_STARLIGHT,                   // cloak of Starlight
    UNRAND_RATSKIN_CLOAK,               // ratskin cloak
    UNRAND_GONG,                        // shield of the Gong
    UNRAND_RCLOUDS,                     // robe of Clouds
    UNRAND_PONDERING,                   // hat of Pondering
    UNRAND_DEMON_AXE,                   // obsidian axe
    UNRAND_LIGHTNING_SCALES,            // lightning scales
    UNRAND_BLACK_KNIGHT_HORSE,          // Black Knight's horse barding
    UNRAND_AUTUMN_KATANA,               // autumn katana
    UNRAND_DEVASTATOR,                  // shillelagh "Devastator"
    UNRAND_WOE,                         // Axe of Woe
    UNRAND_MOON_TROLL_LEATHER_ARMOUR,   // moon troll leather armour
    UNRAND_DARK_MAUL,                   // dark maul
    UNRAND_HIGH_COUNCIL,                // hat of the High Council
    UNRAND_ARC_BLADE,                   // arc blade
    UNRAND_SPELLBINDER,                 // demon whip "Spellbinder"
    UNRAND_ORDER,                       // lajatang of Order
    UNRAND_FIRESTARTER,                 // great mace "Firestarter"
    UNRAND_ORANGE_CRYSTAL_PLATE_ARMOUR, // orange crystal plate armour
    UNRAND_MAJIN,                       // Majin-Bo
    UNRAND_GYRE,                        // pair of quick blades "Gyre" and "Gimble"
    UNRAND_ETHERIC_CAGE,                // Maxwell's etheric cage
    UNRAND_ETERNAL_TORMENT,             // crown of Eternal Torment
    UNRAND_VINES,                       // robe of Vines
    UNRAND_KRYIAS,                      // Kryia's mail coat
    UNRAND_FROSTBITE,                   // frozen axe "Frostbite"
    UNRAND_TALOS,                       // armour of Talos
    UNRAND_WARLOCK_MIRROR,              // warlock's mirror
};

static skill_type _setup_archaeologist_crate(item_def& crate)
{
    item_def unrand;
    unrand_type type;
    do {
        unrand = item_def();
        type = _archa_unrands[random2(_archa_num_unrands)];
        if (!make_item_unrandart(unrand, type))
            continue;

    } while ((!you.could_wield(unrand) && !can_wear_armour(unrand, false, true)) ||  !can_generate(unrand));

    dprf("Initializing archaeologist crate with %s", unrand.name(DESC_A).c_str());
    crate.props[ARCHAEOLOGIST_CRATE_ITEM] = type;

    // Handle items unlocked through interesting skills.
    switch (type)
    {
    case UNRAND_OLGREB:
        return SK_POISON_MAGIC;
    case UNRAND_ELEMENTAL_STAFF:
        return coinflip() ? SK_STAVES : SK_EVOCATIONS;
    case UNRAND_PONDERING:
    case UNRAND_MAXWELL:
    case UNRAND_FOLLY:
        return SK_SPELLCASTING;
    case UNRAND_DRAGONMASK:
    case UNRAND_WAR:
        return SK_FIGHTING;
    case UNRAND_FENCERS:
        return SK_DODGING;
    case UNRAND_THIEF:
    case UNRAND_BOOTS_ASSASSIN:
    case UNRAND_NIGHT:
        return SK_STEALTH;
    default:
        break;
    }

    if (unrand.base_type == OBJ_WEAPONS)
        return item_attack_skill(unrand);
    else if (is_shield(unrand))
        return SK_SHIELDS;
    else if (unrand.sub_type == ARM_ROBE)
        return SK_DODGING;
    else return SK_ARMOUR;
}

static void _setup_archaeologist()
{
    for (uint8_t i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined())
        {
            if (you.inv[i].is_type(OBJ_ARMOUR, ARM_ROBE))
                you.inv[i].props["worn_tile"] = (short)TILEP_BODY_SLIT_BLACK;
            if (you.inv[i].is_type(OBJ_ARMOUR, ARM_GLOVES))
                you.inv[i].props["worn_tile"] = (short)TILEP_ARM_GLOVE_BROWN;
            if (you.inv[i].is_type(OBJ_ARMOUR, ARM_BOOTS))
                you.inv[i].props["worn_tile"] = (short)TILEP_BOOTS_MESH_BLACK;
            if (you.inv[i].is_type(OBJ_ARMOUR, ARM_HAT))
                you.inv[i].props["worn_tile"] = (short)TILEP_HELM_HAT_BLACK;
        }

    skill_type manual_skill;
    for (uint8_t i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined() && you.inv[i].is_type(OBJ_MISCELLANY, MISC_ANCIENT_CRATE))
            manual_skill = _setup_archaeologist_crate(you.inv[i]);

    for (uint8_t i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined() && you.inv[i].is_type(OBJ_MISCELLANY, MISC_DUSTY_TOME))
            you.inv[i].props[ARCHAEOLOGIST_TOME_SKILL] = manual_skill;
}

static void _give_items_skills(const newgame_def& ng)
{
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

    case JOB_CHAOS_KNIGHT:
        you.religion = GOD_XOM;
        you.piety = 100;
        you.gift_timeout = max(5, random2(40) + random2(40));

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;

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

    case JOB_WANDERER:
        create_wanderer();
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

    if (you.char_class == JOB_ARCHAEOLOGIST)
        _setup_archaeologist();
}

static void _give_starting_food()
{
    // No food for those who don't need it.
    if (you_foodless())
        return;

    object_class_type base_type = OBJ_FOOD;
    int sub_type = FOOD_BREAD_RATION;
    int quantity = 1;
    if (you.species == SP_VAMPIRE)
    {
        base_type = OBJ_POTIONS;
        sub_type  = POT_BLOOD;
    }
    else if (player_mutation_level(MUT_CARNIVOROUS))
        sub_type = FOOD_MEAT_RATION;

    // Give another one for hungry species.
    if (player_mutation_level(MUT_FAST_METABOLISM))
        quantity = 2;

    newgame_make_item(base_type, sub_type, quantity);
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

    for (const item_def& i : you.inv)
        if (i.base_type == OBJ_BOOKS)
            mark_had_book(i);

    // Recognisable by appearance.
    you.type_ids[OBJ_POTIONS][POT_BLOOD] = true;

    // Removed item types are handled in _set_removed_types_as_identified.
}

static void _setup_normal_game();
static void _setup_tutorial(const newgame_def& ng);
static void _setup_sprint(const newgame_def& ng);
static void _setup_hints();
static void _setup_generic(const newgame_def& ng);

// Initialise a game based on the choice stored in ng.
void setup_game(const newgame_def& ng)
{
    crawl_state.type = ng.type;
    crawl_state.map  = ng.map;

    switch (crawl_state.type)
    {
    case GAME_TYPE_NORMAL:
        _setup_normal_game();
        break;
    case GAME_TYPE_TUTORIAL:
        _setup_tutorial(ng);
        break;
    case GAME_TYPE_SPRINT:
        _setup_sprint(ng);
        break;
    case GAME_TYPE_HINTS:
        _setup_hints();
        break;
    case GAME_TYPE_ARENA:
    default:
        die("Bad game type");
        end(-1);
    }

    _setup_generic(ng);
}

/**
 * Special steps that normal game needs;
 */
static void _setup_normal_game()
{
    make_hungry(0, true);
}

/**
 * Special steps that tutorial game needs;
 */
static void _setup_tutorial(const newgame_def& ng)
{
    make_hungry(0, true);
}

/**
 * Special steps that sprint needs;
 */
static void _setup_sprint(const newgame_def& ng)
{
    // nothing currently
}

/**
 * Special steps that hints mode needs;
 */
static void _setup_hints()
{
    init_hints();
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

static void _setup_generic(const newgame_def& ng)
{
    _init_player();

#if TAG_MAJOR_VERSION == 34
    // Avoid the remove_dead_shops() Gozag fixup in new games: see
    // ShoppingList::item_type_identified().
    you.props[REMOVED_DEAD_SHOPS_KEY] = true;
#endif

    you.your_name  = ng.name;
    you.species    = ng.species;
    you.char_class = ng.job;

    you.chr_class_name = get_job_name(you.char_class);

    species_stat_init(you.species);     // must be down here {dlb}

    // Before we get into the inventory init, set light radius based
    // on species vision. Currently, all species see out to 8 squares.
    update_vision_range();

    job_stat_init(you.char_class);

    _unfocus_stats();

    // Needs to be done before handing out food.
    give_basic_mutations(you.species);

    // This function depends on stats and mutations being finalised.
    _give_items_skills(ng);

    if (you.species == SP_DEMONSPAWN)
        roll_demonspawn_mutations();

    _give_starting_food();

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

    initialise_item_descriptions();

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
    init_can_train();
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

    initialise_branch_depths();
    initialise_temples();
    init_level_connectivity();

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

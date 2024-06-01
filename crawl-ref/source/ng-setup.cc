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
#include "spl-damage.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "tag-version.h"
#include "throw.h"
#include "transform.h"

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

    // To mitigate higher shield penalties for smaller races, give small
    // races bucklers instead of shields.
    if (item.is_type(OBJ_ARMOUR, ARM_KITE_SHIELD) && you.body_size() < SIZE_MEDIUM)
        item.sub_type = ARM_BUCKLER;

    // If the character is restricted in wearing the requested armour,
    // hand out a replacement instead.
    if (item.base_type == OBJ_ARMOUR
        && !can_wear_armour(item, false, true))
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

    // Make sure branded and/or enchanted items appear as such.
    item_set_appearance(item);

    if ((item.base_type == OBJ_WEAPONS && can_wield(&item, false, false)
        || item.base_type == OBJ_ARMOUR && can_wear_armour(item, false, false))
        && you.equip[get_item_slot(item)] == -1)
    {
        you.equip[get_item_slot(item)] = slot;
    }
    else if (item.base_type == OBJ_WEAPONS
             && you.species == SP_COGLIN
             && you.hands_reqd(item) == HANDS_ONE
             && you.equip[EQ_OFFHAND] == -1
             && you.weapon() // should always be true here
             && you.hands_reqd(*you.weapon()) == HANDS_ONE)
    {
        you.equip[EQ_OFFHAND] = slot;
    }

    if (item.base_type == OBJ_MISSILES)
        _autopickup_ammo(static_cast<missile_type>(item.sub_type));

    if (item.base_type == OBJ_MISCELLANY)
        handle_generated_misc(static_cast<misc_item_type>(item.sub_type));

    origin_set_startequip(item);
    if (item.base_type == OBJ_WEAPONS && you.species == SP_COGLIN)
        name_weapon(item);

    return &item;
}

void give_throwing_ammo(int n)
{
    if (species::can_throw_large_rocks(you.species))
        newgame_make_item(OBJ_MISSILES, MI_LARGE_ROCK, n);
    else if (you.body_size(PSIZE_TORSO) <= SIZE_SMALL)
        newgame_make_item(OBJ_MISSILES, MI_BOOMERANG, 2*n);
    else
        newgame_make_item(OBJ_MISSILES, MI_JAVELIN, n);
}

static void _give_job_spells(job_type job)
{
    vector<spell_type> spells = get_job_spells(job);
    if (spells.empty())
        return;

    if (you.has_mutation(MUT_INNATE_CASTER))
    {
        for (spell_type s : spells)
            if (you.spell_no < MAX_DJINN_SPELLS && !spell_is_useless(s, false))
                add_spell_to_memory(s);
        return;
    }

    library_add_spells(spells, true);

    const spell_type first_spell = spells[0];
    if (!spell_is_useless(first_spell, false, true)
        && spell_difficulty(first_spell) <= 1)
    {
        add_spell_to_memory(first_spell);
    }
}

static void _give_offhand_weapon()
{
    const item_def *wpn = you.weapon();
    if (!wpn || you.shield() || you.hands_reqd(*wpn) != HANDS_ONE)
        return;
    if (is_range_weapon(*wpn))
    {
        const int plus = you.char_class == JOB_HUNTER ? 0 : -2;
        newgame_make_item(OBJ_WEAPONS, WPN_SLING, 1, plus);
    }
    else
        newgame_make_item(OBJ_WEAPONS, WPN_DAGGER);
}

void give_items_skills(const newgame_def& ng)
{
    create_wanderer();

    switch (you.char_class)
    {
    case JOB_BERSERKER:
        you.religion = GOD_TROG;
        you.piety = 35;

        if (you_can_wear(EQ_BODY_ARMOUR) != false)
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

    case JOB_CINDER_ACOLYTE:
        you.religion = GOD_IGNIS;
        you.piety = 150;
        break;

    default:
        break;
    }

    if (you.char_class == JOB_CHAOS_KNIGHT)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, 0, SPWPN_CHAOS);
    else if (you.char_class == JOB_CINDER_ACOLYTE)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, -1, SPWPN_FLAMING);
    else if (job_has_weapon_choice(you.char_class))
        newgame_make_item(OBJ_WEAPONS, ng.weapon);

    give_job_equipment(you.char_class);
    give_job_skills(you.char_class);
    _give_job_spells(you.char_class);
    if (you.char_class == JOB_GLADIATOR)
        give_throwing_ammo(1);

    if (you.has_mutation(MUT_NO_GRASPING)) // i.e. felids
        you.skills[SK_THROWING] = 0;

    if (you.has_mutation(MUT_NO_ARMOUR)) // i.e. felids
        you.skills[SK_SHIELDS] = 0; // i.e. FeFi

    if (you.has_mutation(MUT_WIELD_OFFHAND))
    {
        // Coglins would rather have two slings than one bow.
        if (you.char_class == JOB_HUNTER)
        {
            item_def *wpn = you.weapon();
            wpn->sub_type = WPN_SLING;
            wpn->plus = 2;
        }
        _give_offhand_weapon();
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
    mark_inventory_sets_unknown();

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
    case GAME_TYPE_DESCENT:
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

static bool _spell_triggered_by(spell_type to_trigger, spell_type trigger)
{
    switch (to_trigger)
    {
        case SPELL_BATTLESPHERE:
            return battlesphere_can_mirror(trigger);
        case SPELL_SPELLFORGED_SERVITOR:
            return spell_servitorable(trigger);
        default:
            return true;
    }
}

static bool _spell_has_trigger(spell_type to_trigger,
                               const set<spell_type> &triggers)
{
    for (spell_type trigger : triggers)
        if (_spell_triggered_by(to_trigger, trigger))
            return true;
    return _spell_triggered_by(to_trigger, SPELL_NO_SPELL);
}

static void _setup_innate_spells()
{
    set<spell_type> spellset;
    // Start with all spells from your job.
    for (spell_type sp : you.spells)
        if (sp != SPELL_NO_SPELL)
            spellset.insert(sp);

    // Get spells at XL 3 and every odd level thereafter.
    vector<spell_type> chosen_spells;
    int const min_lev[] = {1,2, 2,3,4, 5,6,6, 6,7,7, 8,9};
    int const max_lev[] = {1,2, 3,4,5, 5,6,7, 7,8,8, 9,9};
    for (int i = 0; i < 27 / 2; i++)
    {
        spell_type next_spell = SPELL_NO_SPELL;
        int seen = 0;
        for (int s = 0; s < NUM_SPELLS; ++s)
        {
            const spell_type spell = static_cast<spell_type>(s);

            if (!is_player_book_spell(spell)
                || spellset.find(spell) != spellset.end()
                || spell_is_useless(spell, false)
                || !_spell_has_trigger(spell, spellset))
            {
                continue;
            }

            const int lev = spell_difficulty(spell);
            if (lev >= min_lev[i] && lev <= max_lev[i]
                && one_chance_in(++seen))
            {
                next_spell = spell;
            }
        }
        ASSERT(next_spell != SPELL_NO_SPELL);
        spellset.insert(next_spell);
        chosen_spells.push_back(next_spell);
    }

    for (spell_type s : chosen_spells)
        you.props[INNATE_SPELLS_KEY].get_vector().push_back(s);
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
    you.deterministic_levelgen =
                            Options.pregen_dungeon != level_gen_type::classic;

#if TAG_MAJOR_VERSION == 34
    // Avoid the remove_dead_shops() Gozag fixup in new games: see
    // ShoppingList::item_type_identified().
    you.props[REMOVED_DEAD_SHOPS_KEY] = true;
#endif

    // Needs to happen before we give the player items, so that item specs
    // in job descriptions can be parsed safely.
    initialise_item_sets();

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
    if (you.has_mutation(MUT_MULTILIVED))
        you.lives = 1;

    if (crawl_state.game_is_sprint())
        _give_bonus_items();

    // Give tutorial skills etc
    if (crawl_state.game_is_tutorial())
        _setup_tutorial_miscs();

    _give_basic_knowledge();

    // intentionally create the subgenerator either way, so that this has the
    // same impact on the current main rng for all chars.
    rng::subgenerator dj_rng;
    if (you.has_mutation(MUT_INNATE_CASTER))
        _setup_innate_spells();

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

    if (you.char_class == JOB_SHAPESHIFTER)
    {
        const item_def* talisman = nullptr;
        for (auto& item : you.inv)
        {
            if (item.is_type(OBJ_TALISMANS, TALISMAN_BEAST))
            {
                talisman = &item;
                break;
            }
        }
        ASSERT(talisman);
        set_default_form(transformation::beast, talisman);
        set_form(transformation::beast, 1); // hacky...
    }

    reassess_starting_skills();
    init_skill_order();
    init_can_currently_train();
    init_train();
    if (you.religion == GOD_TROG)
        join_trog_skills();
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

    // pregen temple -- it's quick and easy, and this prevents a popup from
    // happening. This needs to happen after you.save is created.
    if (normal_dungeon_setup && you.deterministic_levelgen &&
        !crawl_state.game_is_descent() && // disables temple
        !pregen_dungeon(level_id(BRANCH_TEMPLE, 1)))
    {
        die("Builder failure while trying to generate temple!");
    }
}

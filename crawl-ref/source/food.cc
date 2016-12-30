/**
 * @file
 * @brief Functions for eating.
**/

#include "AppHdr.h"

#include "food.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "butcher.h"
#include "chardump.h"
#include "database.h"
#include "delay.h"
#include "env.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "religion.h"
#include "rot.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "travel.h"
#include "transform.h"
#include "xom.h"

static void _eat_chunk(item_def& food);
static void _describe_food_change(int hunger_increment);
static bool _vampire_consume_corpse(item_def& corpse);
static void _heal_from_food(int hp_amt);

void make_hungry(int hunger_amount, bool suppress_msg,
                 bool magic)
{
    if (crawl_state.disables[DIS_HUNGER])
        return;

#if TAG_MAJOR_VERSION == 34
    // Lich/tree form djinn don't get exempted from food costs: infinite
    // healing from channeling would be just too good.
    if (you.species == SP_DJINNI)
    {
        if (!magic)
            return;

        contaminate_player(hunger_amount * 4 / 3, true);
        return;
    }
#endif

    if (you_foodless())
        return;

    if (magic)
        hunger_amount = calc_hunger(hunger_amount);

    if (hunger_amount == 0 && !suppress_msg)
        return;

    you.hunger -= hunger_amount;

    if (you.hunger < 0)
        you.hunger = 0;

    // So we don't get two messages, ever.
    bool state_message = food_change();

    if (!suppress_msg && !state_message)
        _describe_food_change(-hunger_amount);
}

/**
 * Attempt to reduce the player's hunger.
 *
 * @param satiated_amount       The amount by which to reduce hunger by.
 * @param suppress_msg          Whether to squelch messages about hunger
 *                              decreasing.
 * @param max                   The maximum hunger state which the player may
 *                              reach. If -1, defaults to HUNGER_MAXIMUM.
 */
void lessen_hunger(int satiated_amount, bool suppress_msg, int max)
{
    if (you_foodless())
        return;

    you.hunger += satiated_amount;

    const hunger_state_t max_hunger_state = max == -1 ? HS_ENGORGED
                                                      : (hunger_state_t) max;
    ASSERT_RANGE(max_hunger_state, 0, HS_ENGORGED + 1);
    const int max_hunger = min(HUNGER_MAXIMUM,
                               hunger_threshold[max_hunger_state]);
    if (you.hunger > max_hunger)
        you.hunger = max_hunger;

    // So we don't get two messages, ever.
    const bool state_message = food_change();

    if (!suppress_msg && !state_message)
        _describe_food_change(satiated_amount);
}

void set_hunger(int new_hunger_level, bool suppress_msg)
{
    if (you_foodless())
        return;

    int hunger_difference = (new_hunger_level - you.hunger);

    if (hunger_difference < 0)
        make_hungry(-hunger_difference, suppress_msg);
    else if (hunger_difference > 0)
        lessen_hunger(hunger_difference, suppress_msg);
}

bool you_foodless(bool can_eat)
{
    return you.undead_state() == US_UNDEAD
#if TAG_MAJOR_VERSION == 34
        || you.species == SP_DJINNI && !can_eat
#endif
        ;
}

bool you_foodless_normally()
{
    return you.undead_state(false) == US_UNDEAD
#if TAG_MAJOR_VERSION == 34
        || you.species == SP_DJINNI
#endif
        ;
}

bool prompt_eat_item(int slot)
{
    // There's nothing in inventory that a vampire can 'e', and floor corpses
    // are handled by prompt_eat_chunks.
    if (you.species == SP_VAMPIRE)
        return false;

    item_def* item = nullptr;
    if (slot == -1)
    {
        item = use_an_item(OBJ_FOOD, OPER_EAT, "Eat which item?");
        if (!item)
            return false;
    }
    else
        item = &you.inv[slot];

    ASSERT(item);
    if (!can_eat(*item, false))
        return false;

    eat_item(*item);

    return true;
}

static bool _eat_check(bool check_hunger = true, bool silent = false)
{
    if (you_foodless(true))
    {
        if (!silent)
        {
            mpr("You can't eat.");
            crawl_state.zero_turns_taken();
        }
        return false;
    }

    if (!check_hunger)
        return true;

    if (you.hunger_state >= HS_ENGORGED)
    {
        if (!silent)
        {
            mprf("You're too full to %s anything.",
                 you.species == SP_VAMPIRE ? "drain" : "eat");
            crawl_state.zero_turns_taken();
        }
        return false;
    }
    return true;
}

// [ds] Returns true if something was eaten.
bool eat_food(int slot)
{
    if (!_eat_check())
        return false;

    // Skip the prompts if we already know what we're eating.
    if (slot == -1)
    {
        int result = prompt_eat_chunks();
        if (result == 1)
            return true;
        else if (result == -1)
            return false;
    }

    if (you.species == SP_VAMPIRE)
        mpr("There's nothing here to drain!");

    return prompt_eat_item(slot);
}

static string _how_hungry()
{
    if (you.hunger_state > HS_SATIATED)
        return "full";
    else if (you.species == SP_VAMPIRE)
        return "thirsty";
    return "hungry";
}

// "initial" is true when setting the player's initial hunger state on game
// start or load: in that case it's not really a change, so we suppress the
// state change message and don't identify rings or stimulate Xom.
bool food_change(bool initial)
{
    bool state_changed = false;
    bool less_hungry   = false;

    you.hunger = max(you_min_hunger(), you.hunger);
    you.hunger = min(you_max_hunger(), you.hunger);

    // Get new hunger state.
    hunger_state_t newstate = HS_FAINTING;
    while (newstate < HS_ENGORGED && you.hunger > hunger_threshold[newstate])
        newstate = (hunger_state_t)(newstate + 1);

    if (newstate != you.hunger_state)
    {
        state_changed = true;
        if (newstate > you.hunger_state)
            less_hungry = true;

        you.hunger_state = newstate;
        you.redraw_status_lights = true;

        if (newstate < HS_SATIATED)
            interrupt_activity(AI_HUNGRY);

        if (you.species == SP_VAMPIRE)
        {
            const undead_form_reason form_reason = lifeless_prevents_form();
            if (form_reason == UFR_GOOD)
            {
                if (newstate == HS_ENGORGED && is_vampire_feeding()) // Alive
                {
                    print_stats();
                    mpr("You can't stomach any more blood right now.");
                }
            }
            else if (you.duration[DUR_TRANSFORMATION])
            {
                print_stats();
                mprf(MSGCH_WARN,
                     "Your blood-%s body can't sustain your transformation.",
                     form_reason == UFR_TOO_DEAD ? "deprived" : "filled");
                you.duration[DUR_TRANSFORMATION] = 1; // end at end of turn
                // could maybe end immediately, but that makes me nervous
            }
        }

        if (!initial)
        {
            string msg = "You ";
            switch (you.hunger_state)
            {
            case HS_FAINTING:
                msg += "are fainting from starvation!";
                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());
                break;

            case HS_STARVING:
                if (you.species == SP_VAMPIRE)
                    msg += "feel devoid of blood!";
                else
                    msg += "are starving!";

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                learned_something_new(HINT_YOU_STARVING);
                you.check_awaken(500);
                break;

            case HS_NEAR_STARVING:
                if (you.species == SP_VAMPIRE)
                    msg += "feel almost devoid of blood!";
                else
                    msg += "are near starving!";

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                learned_something_new(HINT_YOU_HUNGRY);
                break;

            case HS_VERY_HUNGRY:
            case HS_HUNGRY:
                msg += "are feeling ";
                if (you.hunger_state == HS_VERY_HUNGRY)
                    msg += "very ";
                msg += _how_hungry();
                msg += ".";

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                learned_something_new(HINT_YOU_HUNGRY);
                break;

            default:
                return state_changed;
            }
        }
    }

    return state_changed;
}

// food_increment is positive for eating, negative for hungering
static void _describe_food_change(int food_increment)
{
    const int magnitude = abs(food_increment);
    string msg;

    if (magnitude == 0)
        return;

    msg = "You feel ";

    if (magnitude <= 100)
        msg += "slightly ";
    else if (magnitude <= 350)
        msg += "somewhat ";
    else if (magnitude <= 800)
        msg += "quite a bit ";
    else
        msg += "a lot ";

    if ((you.hunger_state > HS_SATIATED) ^ (food_increment < 0))
        msg += "more ";
    else
        msg += "less ";

    msg += _how_hungry().c_str();
    msg += ".";
    mpr(msg);
}

bool eat_item(item_def &food)
{
    if (food.is_type(OBJ_CORPSES, CORPSE_BODY))
    {
        if (you.species != SP_VAMPIRE)
            return false;

        if (_vampire_consume_corpse(food))
        {
            count_action(CACT_EAT, -1); // subtype Corpse
            you.turn_is_over = true;
            return true;
        }

        return false;
    }

    int eat_time = food_turns(food);
    start_delay<EatDelay>(eat_time - 1, food);

    mprf("You %s %s%s.", eat_time == 1 ? "eat" : "start eating",
                         food.quantity > 1 ? "one of " : "",
                         food.name(DESC_THE).c_str());
    you.turn_is_over = true;
    return true;
}

// Handle messaging at the end of eating.
// Some food types may not get a message.
static void _finished_eating_message(food_type type)
{
    bool herbivorous = player_mutation_level(MUT_HERBIVOROUS) > 0;
    bool carnivorous = player_mutation_level(MUT_CARNIVOROUS) > 0;

    if (herbivorous)
    {
        if (food_is_meaty(type))
        {
            mpr("Blech - you need greens!");
            return;
        }
    }
    else
    {
        switch (type)
        {
        case FOOD_MEAT_RATION:
            mpr("That meat ration really hit the spot!");
            return;
        case FOOD_BEEF_JERKY:
            mprf("That beef jerky was %s!",
                 one_chance_in(4) ? "jerk-a-riffic"
                                  : "delicious");
            return;
        default:
            break;
        }
    }

    if (carnivorous)
    {
        if (food_is_veggie(type))
        {
            mpr("Blech - you need meat!");
            return;
        }
    }
    else
    {
        switch (type)
        {
        case FOOD_BREAD_RATION:
            mpr("That bread ration really hit the spot!");
            return;
        case FOOD_FRUIT:
        {
            string taste = getMiscString("eating_fruit");
            if (taste.empty())
                taste = "Eugh, buggy fruit.";
            mpr(taste);
            break;
        }
        default:
            break;
        }
    }

    switch (type)
    {
    case FOOD_ROYAL_JELLY:
        mpr("That royal jelly was delicious!");
        break;
    case FOOD_PIZZA:
    {
        if (!Options.pizzas.empty())
        {
            const string za = Options.pizzas[random2(Options.pizzas.size())];
            mprf("Mmm... %s.", trimmed_string(za).c_str());
            break;
        }

        const string taste = getMiscString("eating_pizza");
        if (taste.empty())
        {
            mpr("Bleh, bug pizza.");
            break;
        }

        mprf("%s", taste.c_str());
        break;
    }
    default:
        break;
    }
}


void finish_eating_item(item_def& food)
{
    if (food.sub_type == FOOD_CHUNK)
        _eat_chunk(food);
    else
    {
        int value = food_value(food);
        ASSERT(value > 0);
        lessen_hunger(value, true);
        _finished_eating_message(static_cast<food_type>(food.sub_type));
    }

    count_action(CACT_EAT, food.sub_type);

    if (is_perishable_stack(food)) // chunks
        remove_oldest_perishable_item(food);
    if (in_inventory(food))
        dec_inv_item_quantity(food.link, 1);
    else
        dec_mitm_item_quantity(food.index(), 1);
}

// Returns which of two food items is older (true for first, else false).
static bool _compare_by_freshness(const item_def *food1, const item_def *food2)
{
    ASSERT(food1->base_type == OBJ_CORPSES || food1->base_type == OBJ_FOOD);
    ASSERT(food2->base_type == OBJ_CORPSES || food2->base_type == OBJ_FOOD);
    ASSERT(food1->base_type == food2->base_type);

    if (is_inedible(*food1))
        return false;

    if (is_inedible(*food2))
        return true;

    // Permafood can last longest, skip it if possible.
    if (food1->base_type == OBJ_FOOD && food1->sub_type != FOOD_CHUNK)
        return false;
    if (food2->base_type == OBJ_FOOD && food2->sub_type != FOOD_CHUNK)
        return true;

    // At this point, we know both are corpses or chunks, edible

    // Always offer poisonous/mutagenic chunks last.
    if (is_bad_food(*food1) && !is_bad_food(*food2))
        return false;
    if (is_bad_food(*food2) && !is_bad_food(*food1))
        return true;

    return food1->freshness < food2->freshness;
}

/** Make the prompt for chunk eating/corpse draining.
 *
 *  @param only_auto Don't actually make a prompt: if there are
 *                   things to auto_eat, eat them, and exit otherwise.
 *  @returns -1 for cancel, 1 for eaten, 0 for not eaten,
 */
int prompt_eat_chunks(bool only_auto)
{
    // Full herbivores cannot eat chunks.
    if (player_mutation_level(MUT_HERBIVOROUS) == 3)
        return 0;

    // If we *know* the player can eat chunks, doesn't have the gourmand
    // effect and isn't hungry, don't prompt for chunks.
    if (you.species != SP_VAMPIRE
        && you.hunger_state >= HS_SATIATED + player_likes_chunks())
    {
        return 0;
    }

    bool found_valid = false;
    vector<item_def *> chunks;

    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (you.species == SP_VAMPIRE)
        {
            if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
                continue;

            if (!mons_has_blood(si->mon_type))
                continue;
        }
        else if (si->base_type != OBJ_FOOD
                 || si->sub_type != FOOD_CHUNK
                 || is_bad_food(*si))
        {
            continue;
        }

        found_valid = true;
        chunks.push_back(&(*si));
    }

    // Then search through the inventory.
    for (auto &item : you.inv)
    {
        if (!item.defined())
            continue;

        // Vampires can't eat anything in their inventory.
        if (you.species == SP_VAMPIRE)
            continue;

        if (item.base_type != OBJ_FOOD || item.sub_type != FOOD_CHUNK)
            continue;

        // Don't prompt for bad food types.
        if (is_bad_food(item))
            continue;

        found_valid = true;
        chunks.push_back(&item);
    }

    const bool easy_eat = Options.easy_eat_chunks || only_auto;

    if (found_valid)
    {
        sort(chunks.begin(), chunks.end(), _compare_by_freshness);
        for (item_def *item : chunks)
        {
            bool autoeat = false;
            string item_name = menu_colour_item_name(*item, DESC_A);

            const bool bad = is_bad_food(*item);

            // Allow undead to use easy_eat, but not auto_eat, since the player
            // might not want to drink blood as a vampire and might want to save
            // chunks as a ghoul. Ghouls can auto_eat if they have rotted hp.
            const bool no_auto = you.undead_state()
                && !(you.species == SP_GHOUL && player_rotted());

            // If this chunk is safe to eat, just do so without prompting.
            if (easy_eat && !bad && i_feel_safe() && !(only_auto && no_auto))
                autoeat = true;
            else if (only_auto)
                return 0;
            else
            {
                mprf(MSGCH_PROMPT, "%s %s%s? (ye/n/q)",
                     (you.species == SP_VAMPIRE ? "Drink blood from" : "Eat"),
                     ((item->quantity > 1) ? "one of " : ""),
                     item_name.c_str());
            }

            int keyin = autoeat ? 'y' : toalower(getchm(KMC_CONFIRM));
            switch (keyin)
            {
            case 'q':
            CASE_ESCAPE
                canned_msg(MSG_OK);
                return -1;
            case 'i':
            case '?':
                // Skip ahead to the inventory.
                return 0;
            case 'e':
            case 'y':
                if (can_eat(*item, false))
                {
                    if (autoeat)
                    {
                        mprf("%s %s%s.",
                             (you.species == SP_VAMPIRE ? "Drinking blood from"
                                                        : "Eating"),
                             ((item->quantity > 1) ? "one of " : ""),
                             item_name.c_str());
                    }

                    return eat_item(*item) ? 1 : 0;
                }
                break;
            default:
                // Else no: try next one.
                break;
            }
        }
    }

    return 0;
}

static const char *_chunk_flavour_phrase(bool likes_chunks)
{
    const char *phrase = "tastes terrible.";

    if (you.species == SP_GHOUL)
        phrase = "tastes great!";
    else if (likes_chunks)
        phrase = "tastes great.";
    else
    {
        const int gourmand = you.duration[DUR_GOURMAND];
        if (gourmand >= GOURMAND_MAX)
        {
            phrase = one_chance_in(1000) ? "tastes like chicken!"
                                         : "tastes great.";
        }
        else if (gourmand > GOURMAND_MAX * 75 / 100)
            phrase = "tastes very good.";
        else if (gourmand > GOURMAND_MAX * 50 / 100)
            phrase = "tastes good.";
        else if (gourmand > GOURMAND_MAX * 25 / 100)
            phrase = "is not very appetising.";
    }

    return phrase;
}

static void _chunk_nutrition_message(int nutrition)
{
    int perc_nutrition = nutrition * 100 / CHUNK_BASE_NUTRITION;
    if (perc_nutrition < 15)
        mpr("That was extremely unsatisfying.");
    else if (perc_nutrition < 35)
        mpr("That was not very filling.");
}

static int _apply_herbivore_nutrition_effects(int nutrition)
{
    int how_herbivorous = player_mutation_level(MUT_HERBIVOROUS);

    while (how_herbivorous--)
        nutrition = nutrition * 75 / 100;

    return nutrition;
}

static int _apply_gourmand_nutrition_effects(int nutrition, int gourmand)
{
    return nutrition * (gourmand + GOURMAND_NUTRITION_BASE)
                     / (GOURMAND_MAX + GOURMAND_NUTRITION_BASE);
}

static int _chunk_nutrition(int likes_chunks)
{
    int nutrition = CHUNK_BASE_NUTRITION;

    if (you.hunger_state < HS_SATIATED + likes_chunks)
    {
        return likes_chunks ? nutrition
                            : _apply_herbivore_nutrition_effects(nutrition);
    }

    const int gourmand = you.gourmand() ? you.duration[DUR_GOURMAND] : 0;
    const int effective_nutrition =
        _apply_gourmand_nutrition_effects(nutrition, gourmand);

#ifdef DEBUG_DIAGNOSTICS
    const int epercent = effective_nutrition * 100 / nutrition;
    mprf(MSGCH_DIAGNOSTICS,
            "Gourmand factor: %d, chunk base: %d, effective: %d, %%: %d",
                gourmand, nutrition, effective_nutrition, epercent);
#endif

    return _apply_herbivore_nutrition_effects(effective_nutrition);
}

/**
 * How intelligent was the monster that the given corpse came from?
 *
 * @param   The corpse being examined.
 * @return  The mon_intel_type of the monster that the given corpse was
 *          produced from.
 */
mon_intel_type corpse_intelligence(const item_def &corpse)
{
    // An optimising compiler can assume an enum value is in range, so
    // check the range on the uncast value.
    const bool bad = corpse.orig_monnum < 0
                     || corpse.orig_monnum >= NUM_MONSTERS;
    const monster_type orig_mt = static_cast<monster_type>(corpse.orig_monnum);
    const monster_type type = bad || invalid_monster_type(orig_mt)
                                ? corpse.mon_type
                                : orig_mt;
    return mons_class_intel(type);
}

// Never called directly - chunk_effect values must pass
// through food:determine_chunk_effect() first. {dlb}:
static void _eat_chunk(item_def& food)
{
    const corpse_effect_type chunk_effect = determine_chunk_effect(food);

    int likes_chunks  = player_likes_chunks(true);
    int nutrition     = _chunk_nutrition(likes_chunks);
    bool suppress_msg = false; // do we display the chunk nutrition message?
    bool do_eat       = false;

    switch (chunk_effect)
    {
    case CE_MUTAGEN:
        mpr("This meat tastes really weird.");
        mutate(RANDOM_MUTATION, "mutagenic meat");
        did_god_conduct(DID_DELIBERATE_MUTATING, 10);
        xom_is_stimulated(100);
        break;

    case CE_CLEAN:
    {
        if (you.species == SP_GHOUL)
        {
            suppress_msg = true;
            const int hp_amt = 1 + random2avg(5 + you.experience_level, 3);
            _heal_from_food(hp_amt);
        }

        mprf("This raw flesh %s", _chunk_flavour_phrase(likes_chunks));
        do_eat = true;
        break;
    }

    case CE_NOXIOUS:
    case CE_NOCORPSE:
        mprf(MSGCH_ERROR, "This flesh (%d) tastes buggy!", chunk_effect);
        break;
    }

    if (do_eat)
    {
        dprf("nutrition: %d", nutrition);
        lessen_hunger(nutrition, true);
        if (!suppress_msg)
            _chunk_nutrition_message(nutrition);
    }
}

bool is_bad_food(const item_def &food)
{
    return is_mutagenic(food) || is_forbidden_food(food) || is_noxious(food);
}

// Returns true if a food item (or corpse) is mutagenic.
bool is_mutagenic(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return false;

    return determine_chunk_effect(food) == CE_MUTAGEN;
}

// Returns true if a food item (or corpse) is totally inedible.
bool is_noxious(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return false;

    return determine_chunk_effect(food) == CE_NOXIOUS;
}

// Returns true if an item of basetype FOOD or CORPSES cannot currently
// be eaten (respecting species and mutations set).
bool is_inedible(const item_def &item)
{
    // Mummies and liches don't eat.
    if (you_foodless(true))
        return true;

    if (item.base_type == OBJ_FOOD // XXX: removeme?
        && !can_eat(item, true, false))
    {
        return true;
    }

    if (item.base_type == OBJ_CORPSES)
    {
        if (item.sub_type == CORPSE_SKELETON)
            return true;

        if (you.species == SP_VAMPIRE)
        {
            if (!mons_has_blood(item.mon_type))
                return true;
        }
        else
        {
            item_def chunk = item;
            chunk.base_type = OBJ_FOOD;
            chunk.sub_type  = FOOD_CHUNK;
            if (is_inedible(chunk))
                return true;
        }
    }

    return false;
}

// As we want to avoid autocolouring the entire food selection, this should
// be restricted to the absolute highlights, even though other stuff may
// still be edible or even delicious.
bool is_preferred_food(const item_def &food)
{
    // Mummies and liches don't eat.
    if (you_foodless(true))
        return false;

    // Vampires don't really have a preferred food type, but they really
    // like blood potions.
    if (you.species == SP_VAMPIRE)
        return is_blood_potion(food);

#if TAG_MAJOR_VERSION == 34
    if (food.is_type(OBJ_POTIONS, POT_PORRIDGE)
        && item_type_known(food)
        && you.species != SP_DJINNI
        )
    {
        return !player_mutation_level(MUT_CARNIVOROUS);
    }
#endif

    if (food.base_type != OBJ_FOOD)
        return false;

    // Poisoned, mutagenic, etc. food should never be marked as "preferred".
    if (is_bad_food(food))
        return false;

    if (player_mutation_level(MUT_CARNIVOROUS) == 3)
        return food_is_meaty(food.sub_type);

    if (player_mutation_level(MUT_HERBIVOROUS) == 3)
        return food_is_veggie(food.sub_type);

    // No food preference.
    return false;
}

/**
 * Is the given food item forbidden to the player by their god?
 *
 * @param food  The food item in question.
 * @return      Whether your god hates you eating it.
 */
bool is_forbidden_food(const item_def &food)
{
    // no food is forbidden to the player who does not yet exist
    if (!crawl_state.need_save)
        return false;

    // Only corpses are only forbidden, now.
    if (food.base_type != OBJ_CORPSES)
        return false;

    // Specific handling for intelligent monsters like Gastronok and Xtahua
    // of a normally unintelligent class.
    if (you_worship(GOD_ZIN) && corpse_intelligence(food) >= I_HUMAN)
        return true;

    return god_hates_eating(you.religion, food.mon_type);
}

/** Can the player eat this item?
 *
 *  @param food the item (must be a corpse or food item)
 *  @param suppress_msg whether to print why you can't eat it
 *  @param check_hunger whether to check how hungry you are currently
 */
bool can_eat(const item_def &food, bool suppress_msg, bool check_hunger)
{
#define FAIL(msg) { if (!suppress_msg) mpr(msg); return false; }
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        FAIL("That's not food!");

    // special case mutagenic chunks to skip hunger checks, as they don't give
    // nutrition and player can get hungry by using spells etc. anyway
    if (is_mutagenic(food))
        check_hunger = false;

    // [ds] These redundant checks are now necessary - Lua might be calling us.
    if (!_eat_check(check_hunger, suppress_msg))
        return false;

    if (is_noxious(food))
        FAIL("It is completely inedible.");

    if (you.species == SP_VAMPIRE)
    {
        if (food.is_type(OBJ_CORPSES, CORPSE_BODY))
            return true;

        FAIL("Blech - you need blood!")
    }
    else if (food.base_type == OBJ_CORPSES)
        return false;

    if (food_is_veggie(food))
    {
        if (player_mutation_level(MUT_CARNIVOROUS) == 3)
            FAIL("Sorry, you're a carnivore.")
        else
            return true;
    }
    else if (food_is_meaty(food))
    {
        if (player_mutation_level(MUT_HERBIVOROUS) == 3)
            FAIL("Sorry, you're a herbivore.")
        else if (food.sub_type == FOOD_CHUNK)
        {
            if (!check_hunger
                || you.hunger_state < HS_SATIATED
                || player_likes_chunks())
            {
                return true;
            }

            FAIL("You aren't quite hungry enough to eat that!")
        }
    }

    // Any food types not specifically handled until here (e.g. meat
    // rations for non-herbivores) are okay.
    return true;
}

/**
 * Determine the 'effective' chunk type for a given piece of carrion (chunk or
 * corpse), for the player.
 * E.g., ghouls treat rotting and mutagenic chunks as normal chunks.
 *
 * @param carrion       The actual chunk or corpse.
 * @return              A chunk type corresponding to the effect eating the
 *                      given item will have on the player.
 */
corpse_effect_type determine_chunk_effect(const item_def &carrion)
{
    return determine_chunk_effect(mons_corpse_effect(carrion.mon_type));
}

/**
 * Determine the 'effective' chunk type for a given input for the player.
 * E.g., ghouls/vampires treat rotting and mutagenic chunks as normal chunks.
 *
 * @param chunktype     The actual chunk type.
 * @return              A chunk type corresponding to the effect eating a chunk
 *                      of the given type will have on the player.
 */
corpse_effect_type determine_chunk_effect(corpse_effect_type chunktype)
{
    switch (chunktype)
    {
    case CE_NOXIOUS:
    case CE_MUTAGEN:
        if (you.species == SP_GHOUL || you.species == SP_VAMPIRE)
            chunktype = CE_CLEAN;
        break;

    default:
        break;
    }

    return chunktype;
}

static bool _vampire_consume_corpse(item_def& corpse)
{
    ASSERT(you.species == SP_VAMPIRE);
    ASSERT(corpse.base_type == OBJ_CORPSES);
    ASSERT(corpse.sub_type == CORPSE_BODY);

    const monster_type mons_type = corpse.mon_type;

    if (!mons_has_blood(mons_type))
    {
        mpr("There is no blood in this body!");
        return false;
    }

    mprf("This %sblood tastes delicious!",
         mons_class_flag(mons_type, M_WARM_BLOOD) ? "warm " : "");

    const int food_value = CHUNK_BASE_NUTRITION
                           * num_blood_potions_from_corpse(mons_type);
    lessen_hunger(food_value, false);

    // this will never matter :)
    if (mons_genus(mons_type) == MONS_ORC)
        did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
    if (mons_class_holiness(mons_type) & MH_HOLY)
        did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2);

    if (mons_skeleton(mons_type) && one_chance_in(3))
    {
        turn_corpse_into_skeleton(corpse);
        item_check();
    }
    else
        dec_mitm_item_quantity(corpse.index(), 1);

    return true;
}

static void _heal_from_food(int hp_amt)
{
    if (hp_amt > 0)
        inc_hp(hp_amt);

    if (player_rotted())
    {
        mpr("You feel more resilient.");
        unrot_hp(1);
    }

    calc_hp();
    calc_mp();
}

int you_max_hunger()
{
    if (you_foodless())
        return HUNGER_DEFAULT;

    // Ghouls can never be full or above.
    if (you.species == SP_GHOUL)
        return hunger_threshold[HS_SATIATED];

    return hunger_threshold[HS_ENGORGED];
}

int you_min_hunger()
{
    // This case shouldn't actually happen.
    if (you_foodless())
        return HUNGER_DEFAULT;

    // Vampires can never starve to death. Ghouls will just rot much faster.
    if (you.undead_state() != US_ALIVE)
        return (HUNGER_FAINTING + HUNGER_STARVING) / 2; // midpoint

    return 0;
}

void handle_starvation()
{
    // Don't faint or die while eating.
    if (current_delay() && current_delay()->is_being_used(nullptr, OPER_EAT))
        return;

    if (!you_foodless() && you.hunger <= HUNGER_FAINTING)
    {
        if (!you.cannot_act() && one_chance_in(40))
        {
            mprf(MSGCH_FOOD, "You lose consciousness!");
            stop_running();

            int turns = 5 + random2(8);
            if (!you.duration[DUR_PARALYSIS])
                take_note(Note(NOTE_PARALYSIS, min(turns, 13), 0, "fainting"));
            you.increase_duration(DUR_PARALYSIS, turns, 13);
            if (you_worship(GOD_XOM))
                xom_is_stimulated(get_tension() > 0 ? 200 : 100);
        }

        if (you.hunger <= 0 && !you.duration[DUR_DEATHS_DOOR])
        {
            auto it = min_element(begin(you.inv), end(you.inv),
                [](const item_def& a, const item_def& b) -> bool
                {
                    return (can_eat(a, true) ? food_turns(a) : INT_MAX)
                         < (can_eat(b, true) ? food_turns(b) : INT_MAX);
                });
            if (it != end(you.inv) && can_eat(*it, true))
            {
                mpr("As you are about to starve, you manage to eat something.");
                eat_item(*it);
                return;
            }

            mprf(MSGCH_FOOD, "You have starved to death.");
            ouch(INSTANT_DEATH, KILLED_BY_STARVATION);
            if (!you.pending_revival) // if we're still here...
                set_hunger(HUNGER_DEFAULT, true);
        }
    }
}

static const int hunger_breakpoints[] = { 1, 21, 61, 121, 201, 301, 421 };

int hunger_bars(const int hunger)
{
    return breakpoint_rank(hunger, hunger_breakpoints,
                           ARRAYSZ(hunger_breakpoints));
}

string hunger_cost_string(const int hunger)
{
    if (you_foodless(true))
        return "N/A";

#ifdef WIZARD
    if (you.wizard)
        return to_string(hunger);
#endif

    const int numbars = hunger_bars(hunger);

    if (numbars > 0)
    {
        return string(numbars, '#')
               + string(ARRAYSZ(hunger_breakpoints) - numbars, '.');
    }
    else
        return "None";
}

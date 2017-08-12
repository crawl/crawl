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

static void _describe_food_change(int hunger_increment);
static bool _vampire_consume_corpse(item_def& corpse);
static void _heal_from_food(int hp_amt);

void make_hungry(int hunger_amount, bool suppress_msg,
                 bool magic)
{
    if (crawl_state.disables[DIS_HUNGER])
        return;

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

bool you_foodless()
{
    return you.undead_state() == US_UNDEAD;
}

bool you_foodless_normally()
{
    return you.undead_state(false) == US_UNDEAD;
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
    if (you_foodless())
    {
        if (!silent)
        {
            mpr("당신은 먹을 수 없다.");
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
            mprf("당신은 %s에는 너무 배부르다.",
                 you.species == SP_VAMPIRE ? "흡혈하기" : "먹기");
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
        mpr("여기엔 흡수할 것이 없다!");

    return prompt_eat_item(slot);
}

static string _how_hungry()
{
    if (you.hunger_state > HS_SATIATED)
        return "배부르다";
    else if (you.species == SP_VAMPIRE)
        return "목마르다";
    return "배고프다";
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
                    mpr("당신은 지금 당장은 더 피를 소화할 수 없다.");
                }
            }
            else if (you.duration[DUR_TRANSFORMATION])
            {
                print_stats();
                mprf(MSGCH_WARN,
                     "당신의 피%s 몸은 당신의 변신을 유지할 수 없다.",
                     form_reason == UFR_TOO_DEAD ? "가 없는" : "로 가득한");
                you.duration[DUR_TRANSFORMATION] = 1; // end at end of turn
                // could maybe end immediately, but that makes me nervous
            }
        }

        if (!initial)
        {
            string msg = "당신은 ";
            switch (you.hunger_state)
            {
            case HS_FAINTING:
                msg += "배고픔으로 기절하기 직전이다!";
                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());
                break;

            case HS_STARVING:
                if (you.species == SP_VAMPIRE)
                    msg += "피가 전혀 없는 것을 느꼈다!";
                else
                    msg += "굶주리고 있다!";

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                learned_something_new(HINT_YOU_STARVING);
                you.check_awaken(500);
                break;

            case HS_NEAR_STARVING:
                if (you.species == SP_VAMPIRE)
                    msg += "피가 거의 없는 것을 느꼈다!";
                else
                    msg += "굶주리기 직전이다!";

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                learned_something_new(HINT_YOU_HUNGRY);
                break;

            case HS_VERY_HUNGRY:
            case HS_HUNGRY:
                msg += "";
                if (you.hunger_state == HS_VERY_HUNGRY)
                    msg += "굉장히 ";
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

    msg = "당신은 ";

    if (magnitude <= 100)
        msg += "약간 ";
    else if (magnitude <= 350)
        msg += "어느정도 ";
    else if (magnitude <= 800)
        msg += "상당히 ";
    else
        msg += "엄청 ";

    if ((you.hunger_state > HS_SATIATED) ^ (food_increment < 0))
        msg += "더 ";
    else
        msg += "조금 ";

    msg += _how_hungry().c_str();
    msg += ".";
    mpr(msg);
}

// Handle messaging at the end of eating.
// Some food types may not get a message.
static void _finished_eating_message(food_type type)
{
    bool herbivorous = you.get_mutation_level(MUT_HERBIVOROUS) > 0;
    bool carnivorous = you.get_mutation_level(MUT_CARNIVOROUS) > 0;

    if (herbivorous)
    {
        if (food_is_meaty(type))
        {
            mpr("우웩 - 당신에겐 채식이 필요하다!");
            return;
        }
    }
    else
    {
        if (type == FOOD_MEAT_RATION)
        {
            mpr("그 빵 식량은 정말 맛있었다!");
            return;
        }
    }

    if (carnivorous)
    {
        if (food_is_veggie(type))
        {
            mpr("우웩 - 당신에겐 고기가 필요하다!");
            return;
        }
    }
    else
    {
        switch (type)
        {
        case FOOD_BREAD_RATION:
            mpr("그 빵 식량은 정말 맛있었다!");
            return;
        case FOOD_FRUIT:
        {
            string taste = getMiscString("eating_fruit");
            if (taste.empty())
                taste = "Eugh, buggy fruit.";
            mpr(taste);
            return;
        }
        default:
            break;
        }
    }

    if (type == FOOD_ROYAL_JELLY)
        mpr("이 로열 젤리는 맛있다!");
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
    if (you.get_mutation_level(MUT_HERBIVOROUS) == 3)
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
                mprf(MSGCH_PROMPT, "%s? : %s%s (ye/n/q)",
                     (you.species == SP_VAMPIRE ? "흡혈하겠는가" : "먹겠는가"),
                     ((item->quantity > 1) ? "중 하나를 " : ""),
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
                        mprf("%s : %s%s.",
                             (you.species == SP_VAMPIRE ? "흡혈했다"
                                                        : "먹었다"),
                             ((item->quantity > 1) ? "중 하나를 " : ""),
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
    const char *phrase = "끔찍한 맛이다.";

    if (you.species == SP_GHOUL)
        phrase = "굉장히 맛있다!";
    else if (likes_chunks)
        phrase = "굉장히 맛있다.";
    else
    {
        const int gourmand = you.duration[DUR_GOURMAND];
        if (gourmand >= GOURMAND_MAX)
        {
            phrase = one_chance_in(1000) ? "닭고기 맛이 난다!"
                                         : "굉장히 맛있다.";
        }
        else if (gourmand > GOURMAND_MAX * 75 / 100)
            phrase = "맛있다.";
        else if (gourmand > GOURMAND_MAX * 50 / 100)
            phrase = "먹을 만하다.";
        else if (gourmand > GOURMAND_MAX * 25 / 100)
            phrase = "별로 맛있진 않다.";
    }

    return phrase;
}

static void _chunk_nutrition_message(int nutrition)
{
    int perc_nutrition = nutrition * 100 / CHUNK_BASE_NUTRITION;
    if (perc_nutrition < 15)
        mpr("이것은 극도로 불만족스럽다.");
    else if (perc_nutrition < 35)
        mpr("별로 만족스럽지 않았다.");
}

static int _apply_herbivore_nutrition_effects(int nutrition)
{
    int how_herbivorous = you.get_mutation_level(MUT_HERBIVOROUS);

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
        mpr("이 고기는 정말 이상한 맛이다.");
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

        mprf("이 날고기는 %s", _chunk_flavour_phrase(likes_chunks));
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

    mprf("당신은 먹었다. : %s%s", food.quantity > 1 ? "중 하나를 " : "",
                          food.name(DESC_PLAIN).c_str());

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

    you.turn_is_over = true;
    return true;
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
    if (you_foodless())
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
    if (you_foodless())
        return false;

    // Vampires don't really have a preferred food type, but they really
    // like blood potions.
    if (you.species == SP_VAMPIRE)
        return is_blood_potion(food);

#if TAG_MAJOR_VERSION == 34
    if (food.is_type(OBJ_POTIONS, POT_PORRIDGE)
        && item_type_known(food))
    {
        return !you.get_mutation_level(MUT_CARNIVOROUS);
    }
#endif

    if (food.base_type != OBJ_FOOD)
        return false;

    // Poisoned, mutagenic, etc. food should never be marked as "preferred".
    if (is_bad_food(food))
        return false;

    if (you.get_mutation_level(MUT_CARNIVOROUS) == 3)
        return food_is_meaty(food.sub_type);

    if (you.get_mutation_level(MUT_HERBIVOROUS) == 3)
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
        if (you.get_mutation_level(MUT_CARNIVOROUS) == 3)
            FAIL("Sorry, you're a carnivore.")
        else
            return true;
    }
    else if (food_is_meaty(food))
    {
        if (you.get_mutation_level(MUT_HERBIVOROUS) == 3)
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
        mpr("이 몸뚱이에는 피가 하나도 없다!");
        return false;
    }

    mprf("이 %s피는 맛있다!",
         mons_class_flag(mons_type, M_WARM_BLOOD) ? "따듯한 " : "");

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
        mpr("더 건강해진 것 같다.");
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
            mprf(MSGCH_FOOD, "당신은 의식을 잃었다!");
            stop_running();

            int turns = 5 + random2(8);
            if (!you.duration[DUR_PARALYSIS])
                take_note(Note(NOTE_PARALYSIS, min(turns, 13), 0, "fainting"));
            you.increase_duration(DUR_PARALYSIS, turns, 13);
            xom_is_stimulated(get_tension() > 0 ? 200 : 100);
        }

        if (you.hunger <= 0 && !you.duration[DUR_DEATHS_DOOR])
        {
            auto it = find_if(begin(you.inv), end(you.inv),
                [](const item_def& food) -> bool
                {
                    return can_eat(food, true);
                });
            if (it != end(you.inv))
            {
                mpr("당신이 굶주릴수록 무언가를 먹어야 할 것이다.");
                eat_item(*it);
                return;
            }

            mprf(MSGCH_FOOD, "당신은 굶주림으로 죽었다.");
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
    if (you_foodless())
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

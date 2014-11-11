/**
 * @file
 * @brief Functions for eating.
**/

#include "AppHdr.h"

#include "food.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "butcher.h"
#include "database.h"
#include "delay.h"
#include "env.h"
#include "godabil.h"
#include "godconduct.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mutation.h"
#include "options.h"
#include "output.h"
#include "religion.h"
#include "rot.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "travel.h"
#include "xom.h"

static void _eat_chunk(item_def& food);
static void _eating(item_def &food);
static void _describe_food_change(int hunger_increment);
static bool _vampire_consume_corpse(int slot, bool invent);
static void _heal_from_food(int hp_amt, bool unrot = false);

void make_hungry(int hunger_amount, bool suppress_msg,
                 bool magic)
{
    if (crawl_state.disables[DIS_HUNGER])
        return;

    if (crawl_state.game_is_zotdef() && you.undead_state() != US_SEMI_UNDEAD)
    {
        you.hunger = HUNGER_DEFAULT;
        you.hunger_state = HS_SATIATED;
        return;
    }
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

#ifdef DEBUG_DIAGNOSTICS
    set_redraw_status(REDRAW_HUNGER);
#endif

    you.hunger -= hunger_amount;

    if (you.hunger < 0)
        you.hunger = 0;

    // So we don't get two messages, ever.
    bool state_message = food_change();

    if (!suppress_msg && !state_message)
        _describe_food_change(-hunger_amount);
}

void lessen_hunger(int satiated_amount, bool suppress_msg)
{
    if (you_foodless())
        return;

    you.hunger += satiated_amount;

    if (you.hunger > HUNGER_MAXIMUM)
        you.hunger = HUNGER_MAXIMUM;

    // So we don't get two messages, ever.
    bool state_message = food_change();

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

bool prompt_eat_inventory_item(int slot)
{
    // There's nothing in inventory that a vampire can 'e'.
    if (you.species == SP_VAMPIRE)
        return false;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return false;
    }

    int which_inventory_slot = slot;

    if (slot == -1)
    {
        which_inventory_slot = prompt_invent_item("Eat which item?",
                                                  MT_INVLIST, OBJ_FOOD,
                                                  true, true, true, 0, -1,
                                                  NULL, OPER_EAT);

        if (prompt_failed(which_inventory_slot))
            return false;
    }

    item_def &item(you.inv[which_inventory_slot]);
    if (item.base_type != OBJ_FOOD)
    {
        mpr("You can't eat that!");
        return false;
    }

    if (!can_eat(item, false))
        return false;

    eat_item(item);
    you.turn_is_over = true;

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
        if (result == 1 || result == -1)
            return result > 0;

        if (result != -2) // else skip ahead to inventory
        {
            if (you.visible_igrd(you.pos()) != NON_ITEM)
            {
                result = eat_from_floor(true);
                if (result == 1)
                    return true;
                if (result == -1)
                    return false;
            }
        }
    }

    if (you.species == SP_VAMPIRE)
        mpr("There's nothing here to drain!");

    return prompt_eat_inventory_item(slot);
}

static string _how_hungry()
{
    if (you.hunger_state > HS_SATIATED)
        return "full";
    else if (you.species == SP_VAMPIRE)
        return "thirsty";
    return "hungry";
}

// Must match the order of hunger_state_t enums
static constexpr int hunger_threshold[HS_ENGORGED + 1] =
    { HUNGER_STARVING, HUNGER_NEAR_STARVING, HUNGER_VERY_HUNGRY, HUNGER_HUNGRY,
      HUNGER_SATIATED, HUNGER_FULL, HUNGER_VERY_FULL, HUNGER_ENGORGED };

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
    hunger_state_t newstate = HS_STARVING;
    while (newstate < HS_ENGORGED && you.hunger > hunger_threshold[newstate])
        newstate = (hunger_state_t)(newstate + 1);

    if (newstate != you.hunger_state)
    {
        state_changed = true;
        if (newstate > you.hunger_state)
            less_hungry = true;

        you.hunger_state = newstate;
        set_redraw_status(REDRAW_HUNGER);

        if (newstate < HS_SATIATED)
            interrupt_activity(AI_HUNGRY);

        if (you.species == SP_VAMPIRE)
        {
            if (newstate <= HS_SATIATED)
            {
                if (you.duration[DUR_BERSERK] > 1 && newstate <= HS_HUNGRY)
                {
                    mprf(MSGCH_DURATION, "Your blood-deprived body can't sustain "
                                         "your rage any longer.");
                    you.duration[DUR_BERSERK] = 1;
                }
                if (you.form != TRAN_NONE && you.form != TRAN_BAT
                    && you.duration[DUR_TRANSFORMATION] > 2 * BASELINE_DELAY)
                {
                    mprf(MSGCH_DURATION, "Your blood-deprived body can't sustain "
                                         "your transformation much longer.");
                    you.set_duration(DUR_TRANSFORMATION, 2);
                }
            }
            else if (you.form == TRAN_BAT
                     && you.duration[DUR_TRANSFORMATION] > 5)
            {
                print_stats();
                mprf(MSGCH_WARN, "Your blood-filled body can't sustain your "
                                 "transformation much longer.");

                // Give more time because suddenly stopping flying can be fatal.
                you.set_duration(DUR_TRANSFORMATION, 5);
            }
            else if (newstate == HS_ENGORGED && is_vampire_feeding()) // Alive
            {
                print_stats();
                mpr("You can't stomach any more blood right now.");
            }
        }

        if (!initial)
        {
            string msg = "You ";
            switch (you.hunger_state)
            {
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
    int magnitude = (food_increment > 0)?food_increment:(-food_increment);
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
    int link;

    if (in_inventory(food))
        link = food.link;
    else
    {
        link = item_on_floor(food, you.pos());
        if (link == NON_ITEM)
            return false;
    }

    if (food.base_type == OBJ_CORPSES && food.sub_type == CORPSE_BODY)
    {
        if (you.species != SP_VAMPIRE)
            return false;

        if (_vampire_consume_corpse(link, in_inventory(food)))
        {
            count_action(CACT_EAT, -1);
            you.turn_is_over = true;
            return true;
        }
        else
            return false;
    }
    else if (food.sub_type == FOOD_CHUNK)
        _eat_chunk(food);
    else
        _eating(food);

    you.turn_is_over = true;

    count_action(CACT_EAT, food.sub_type);
    if (is_perishable_stack(food)) // chunks
        remove_oldest_perishable_item(food);
    if (in_inventory(food))
        dec_inv_item_quantity(link, 1);
    else
        dec_mitm_item_quantity(link, 1);

    return true;
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

    return food1->special < food2->special;
}

#ifdef TOUCH_UI
static string _floor_eat_menu_title(const Menu *menu, const string &oldt)
{
    return oldt;
}
#endif

// Returns -1 for cancel, 1 for eaten, 0 for not eaten.
int eat_from_floor(bool skip_chunks)
{
    if (!_eat_check())
        return false;

    // Corpses should have been handled before.
    if (you.species == SP_VAMPIRE)
        return 0;

    bool need_more = false;
    int inedible_food = 0;
    item_def wonteat;
    bool found_valid = false;

    vector<const item_def*> food_items;
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type != OBJ_FOOD)
            continue;

        // Chunks should have been handled before.
        if (skip_chunks && si->sub_type == FOOD_CHUNK)
            continue;

        if (is_bad_food(*si))
            continue;

        if (!can_eat(*si, true))
        {
            if (!inedible_food)
            {
                wonteat = *si;
                inedible_food++;
            }
            else
            {
                // Increase only if we're dealing with different subtypes.
                // FIXME: Use a common check for herbivorous/carnivorous
                //        dislikes, for e.g. "Blech! You need blood!"
                ASSERT(wonteat.defined());
                if (wonteat.sub_type != si->sub_type)
                    inedible_food++;
            }

            continue;
        }

        found_valid = true;
        food_items.push_back(&(*si));
    }

    if (found_valid)
    {
#ifdef TOUCH_UI
        vector<SelItem> selected =
            select_items(food_items,
                         "Eat",
                         false, MT_SELONE, _floor_eat_menu_title);
        redraw_screen();
        for (int i = 0, count = selected.size(); i < count; ++i)
        {
            item_def *item = const_cast<item_def *>(selected[i].item);
            if (!check_warning_inscriptions(*item, OPER_EAT))
                break;

            if (can_eat(*item, false))
                return eat_item(*item);
        }
#else
        sort(food_items.begin(), food_items.end(), _compare_by_freshness);
        for (unsigned int i = 0; i < food_items.size(); ++i)
        {
            item_def *item = const_cast<item_def *>(food_items[i]);
            string item_name = get_menu_colour_prefix_tags(*item, DESC_A);

            mprf(MSGCH_PROMPT, "%s %s%s? (ye/n/q/i?)",
                 "Eat",
                 ((item->quantity > 1) ? "one of " : ""),
                 item_name.c_str());

            int keyin = toalower(getchm(KMC_CONFIRM));
            switch (keyin)
            {
            case 'q':
            CASE_ESCAPE
                canned_msg(MSG_OK);
                return -1;
            case 'e':
            case 'y':
                if (!check_warning_inscriptions(*item, OPER_EAT))
                    break;

                if (can_eat(*item, false))
                    return eat_item(*item);
                need_more = true;
                break;
            case 'i':
            case '?':
                // Directly skip ahead to inventory.
                return 0;
            default:
                // Else no: try next one.
                break;
            }
        }
#endif
    }
    else if (inedible_food)
    {
        if (inedible_food == 1)
        {
            ASSERT(wonteat.defined());
            // Use the normal cannot ingest message.
            if (can_eat(wonteat, false))
            {
                mprf(MSGCH_DIAGNOSTICS, "Error: Can eat %s after all?",
                     wonteat.name(DESC_PLAIN).c_str());
            }
        }
        else // Several different food items.
            mpr("You refuse to eat these food items.");
        need_more = true;
    }

    if (need_more)
        more();

    return 0;
}

bool eat_from_inventory()
{
    if (!_eat_check())
        return false;

    // Corpses should have been handled before.
    if (you.species == SP_VAMPIRE)
        return 0;

    int inedible_food = 0;
    item_def *wonteat = NULL;
    bool found_valid = false;

    vector<item_def *> food_items;
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].defined())
            continue;

        item_def *item = &you.inv[i];
        // Chunks should have been handled before.
        if (item->base_type != OBJ_FOOD || item->sub_type == FOOD_CHUNK)
            continue;

        if (is_bad_food(*item))
            continue;

        if (!can_eat(*item, true))
        {
            if (!inedible_food)
            {
                wonteat = item;
                inedible_food++;
            }
            else
            {
                // Increase only if we're dealing with different subtypes.
                // FIXME: Use a common check for herbivorous/carnivorous
                //        dislikes, for e.g. "Blech! You need blood!"
                ASSERT(wonteat->defined());
                if (wonteat->sub_type != item->sub_type)
                    inedible_food++;
            }
            continue;
        }

        found_valid = true;
        food_items.push_back(item);
    }

    if (found_valid)
    {
        sort(food_items.begin(), food_items.end(), _compare_by_freshness);
        for (unsigned int i = 0; i < food_items.size(); ++i)
        {
            item_def *item = food_items[i];
            string item_name = get_menu_colour_prefix_tags(*item, DESC_A);

            mprf(MSGCH_PROMPT, "%s %s%s? (ye/n/q)",
                 "Eat",
                 ((item->quantity > 1) ? "one of " : ""),
                 item_name.c_str());

            int keyin = toalower(getchm(KMC_CONFIRM));
            switch (keyin)
            {
            case 'q':
            CASE_ESCAPE
                canned_msg(MSG_OK);
                return false;
            case 'e':
            case 'y':
                if (can_eat(*item, false))
                    return eat_item(*item);
                break;
            default:
                // Else no: try next one.
                break;
            }
        }
    }
    else if (inedible_food)
    {
        if (inedible_food == 1)
        {
            ASSERT(wonteat->defined());
            // Use the normal cannot ingest message.
            if (can_eat(*wonteat, false))
            {
                mprf(MSGCH_DIAGNOSTICS, "Error: Can eat %s after all?",
                    wonteat->name(DESC_PLAIN).c_str());
            }
        }
        else // Several different food items.
            mpr("You refuse to eat these food items.");
    }

    return false;
}


/** Make the prompt for chunk eating/corpse draining.
 *
 *  @param only_auto Don't actually make a prompt: if there are
 *                   things to auto_eat, eat them, and exit otherwise.
 *  @returns -1 for cancel, 1 for eaten, 0 for not eaten,
 *           -2 for skip to inventory.
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
        else if (si->base_type != OBJ_FOOD || si->sub_type != FOOD_CHUNK)
            continue;

        // Don't prompt for bad food types.
        if (is_bad_food(*si))
            continue;

        found_valid = true;
        chunks.push_back(&(*si));
    }

    // Then search through the inventory.
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].defined())
            continue;

        item_def *item = &you.inv[i];

        // Vampires can't eat anything in their inventory.
        if (you.species == SP_VAMPIRE)
            continue;

        if (item->base_type != OBJ_FOOD || item->sub_type != FOOD_CHUNK)
            continue;

        // Don't prompt for bad food types.
        if (is_bad_food(*item))
            continue;

        found_valid = true;
        chunks.push_back(item);
    }

    const bool easy_eat = Options.easy_eat_chunks || only_auto;

    if (found_valid)
    {
        sort(chunks.begin(), chunks.end(), _compare_by_freshness);
        for (unsigned int i = 0; i < chunks.size(); ++i)
        {
            bool autoeat = false;
            item_def *item = chunks[i];
            string item_name = get_menu_colour_prefix_tags(*item, DESC_A);

            const bool bad = is_bad_food(*item);

            // Allow undead to use easy_eat, but not auto_eat, since the player
            // might not want to drink blood as a vampire and might want to save
            // chunks as a ghoul.
            if (easy_eat && !bad && i_feel_safe() && !(only_auto &&
                                                       you.undead_state()))
            {
                // If this chunk is safe to eat, just do so without prompting.
                autoeat = true;
            }
            else if (only_auto)
                return 0;
            else
            {
                mprf(MSGCH_PROMPT, "%s %s%s? (ye/n/q/i?)",
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
                return -2;
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

                    if (eat_item(*item))
                        return 1;
                    else
                        return 0;
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

void chunk_nutrition_message(int nutrition)
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

    case CE_ROT:
        you.rot(&you, 10 + random2(10));
        if (you.sicken(50 + random2(100)))
            xom_is_stimulated(random2(100));
        break;

    case CE_CLEAN:
    {
        if (you.species == SP_GHOUL)
        {
            suppress_msg = true;
            const int hp_amt = 1 + random2avg(5 + you.experience_level, 3);
            _heal_from_food(hp_amt, true);
        }

        mprf("This raw flesh %s", _chunk_flavour_phrase(likes_chunks));
        do_eat = true;
        break;
    }

    case CE_POISONOUS:
    case CE_NOCORPSE:
        mprf(MSGCH_ERROR, "This flesh (%d) tastes buggy!", chunk_effect);
        break;
    }

    if (do_eat)
    {
        dprf("nutrition: %d", nutrition);
        zin_recite_interrupt();
        start_delay(DELAY_EAT, food_turns(food) - 1,
                    (suppress_msg) ? 0 : nutrition, -1);
        lessen_hunger(nutrition, true);
    }
}

static void _eating(item_def& food)
{
    int food_value = ::food_value(food);
    ASSERT(food_value > 0);

    int duration = food_turns(food) - 1;

    // use delay.parm3 to figure out whether to output "finish eating"
    zin_recite_interrupt();
    start_delay(DELAY_EAT, duration, 0, food.sub_type, duration);

    lessen_hunger(food_value, true);
}

// Handle messaging at the end of eating.
// Some food types may not get a message.
void finished_eating_message(int food_type)
{
    bool herbivorous = player_mutation_level(MUT_HERBIVOROUS) > 0;
    bool carnivorous = player_mutation_level(MUT_CARNIVOROUS) > 0;

    if (herbivorous)
    {
        if (food_is_meaty(food_type))
        {
            mpr("Blech - you need greens!");
            return;
        }
    }
    else
    {
        switch (food_type)
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
        if (food_is_veggie(food_type))
        {
            mpr("Blech - you need meat!");
            return;
        }
    }
    else
    {
        switch (food_type)
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

    switch (food_type)
    {
    case FOOD_ROYAL_JELLY:
        mpr("That royal jelly was delicious!");
        break;
    case FOOD_PIZZA:
    {
        string taste = getMiscString("eating_pizza");
        if (taste.empty())
            taste = "Bleh, bug pizza.";
        mpr(taste);
        break;
    }
    default:
        break;
    }
}

// Divide full nutrition by duration, so that each turn you get the same
// amount of nutrition. Also, experimentally regenerate 1 hp per feeding turn
// - this is likely too strong.
// feeding is -1 at start, 1 when finishing, and 0 else

// Here are some values for nutrition (quantity * 1000) and duration:
//    max_chunks      quantity    duration
//     1               1           1
//     2               1           1
//     3               1           2
//     4               1           2
//     5               1           2
//     6               2           3
//     7               2           3
//     8               2           3
//     9               2           4
//    10               2           4
//    12               3           5
//    15               3           5
//    20               4           6
//    25               4           6
//    30               5           7

void vampire_nutrition_per_turn(const item_def &corpse, int feeding)
{
    const monster_type mons_type = corpse.mon_type;
    const corpse_effect_type chunk_type = determine_chunk_effect(corpse);

    // Duration depends on corpse weight.
    const int max_chunks = get_max_corpse_chunks(mons_type);
    int chunk_amount     = 1 + max_chunks/3;
        chunk_amount     = stepdown_value(chunk_amount, 6, 6, 12, 12);

    // Add 1 for the artificial extra call at the start of draining.
    const int duration   = 1 + chunk_amount;

    // Use number of potions per corpse to calculate total nutrition, which
    // then gets distributed over the entire duration.
    int food_value = CHUNK_BASE_NUTRITION
                     * num_blood_potions_from_corpse(mons_type);

    bool start_feeding   = false;
    bool end_feeding     = false;

    if (feeding < 0)
        start_feeding = true;
    else if (feeding > 0)
        end_feeding = true;

    switch (chunk_type)
    {
        case CE_CLEAN:
            if (start_feeding)
            {
                mprf("This %sblood tastes delicious!",
                     mons_class_flag(mons_type, M_WARM_BLOOD) ? "warm "
                                                              : "");
            }
            else if (end_feeding && corpse.special > 150)
                _heal_from_food(1);
            break;

        case CE_MUTAGEN:
            food_value /= 2;
            if (start_feeding)
                mpr("This blood tastes really weird!");
            mutate(RANDOM_MUTATION, "mutagenic blood");
            did_god_conduct(DID_DELIBERATE_MUTATING, 10);
            xom_is_stimulated(100);
            // Sometimes heal by one hp.
            if (end_feeding && corpse.special > 150 && coinflip())
                _heal_from_food(1);
            break;

        case CE_ROT:
            you.rot(&you, 5 + random2(5));
            if (you.sicken(50 + random2(100)))
                xom_is_stimulated(random2(100));
            stop_delay();
            break;

        case CE_POISONOUS:
        case CE_NOCORPSE:
            mprf(MSGCH_ERROR, "This blood (%d) tastes buggy!", chunk_type);
            return;
    }

    if (!end_feeding)
        lessen_hunger(food_value / duration, !start_feeding);
}

bool is_bad_food(const item_def &food)
{
    return is_poisonous(food) || is_mutagenic(food)
           || is_forbidden_food(food) || causes_rot(food);
}

// Returns true if a food item (or corpse) is poisonous AND the player is not
// (known to be) poison resistant.
bool is_poisonous(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return false;

    if (player_res_poison(false) > 0)
        return false;

    return carrion_is_poisonous(food);
}

// Returns true if a food item (or corpse) is mutagenic.
bool is_mutagenic(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return false;

    return determine_chunk_effect(food) == CE_MUTAGEN;
}

// Returns true if a food item (or corpse) will cause rotting.
bool causes_rot(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return false;

    return determine_chunk_effect(food) == CE_ROT;
}

// Returns true if an item of basetype FOOD or CORPSES cannot currently
// be eaten (respecting species and mutations set).
bool is_inedible(const item_def &item)
{
    // Mummies and liches don't eat.
    if (you_foodless(true))
        return true;

    if (item.base_type == OBJ_FOOD
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
    if (food.base_type == OBJ_POTIONS && food.sub_type == POT_PORRIDGE
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
    if (you_worship(GOD_ZIN) && corpse_intelligence(food) >= I_NORMAL)
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
    int sub_type = food.sub_type;
    ASSERT(food.base_type == OBJ_FOOD || food.base_type == OBJ_CORPSES);

    // special case mutagenic chunks to skip hunger checks, as they don't give
    // nutrition and player can get hungry by using spells etc. anyway
    if (is_mutagenic(food))
        check_hunger = false;

    // [ds] These redundant checks are now necessary - Lua might be calling us.
    if (!_eat_check(check_hunger, suppress_msg))
        return false;

    if (check_hunger)
    {
        if (is_poisonous(food))
            FAIL("It contains deadly poison!");
        if (causes_rot(food))
            FAIL("It is caustic! Not only inedible but also greatly harmful!");
    }

    if (you.species == SP_VAMPIRE)
    {
        if (food.base_type == OBJ_CORPSES && sub_type == CORPSE_BODY)
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
        else if (sub_type == FOOD_CHUNK)
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
 * Is a given chunk or corpse poisonous, independent of the player's status?
 *
 * (I.e., excluding resists, holiness, etc - even those from species)
 */
bool carrion_is_poisonous(const item_def &food)
{
    return mons_corpse_effect(food.mon_type) == CE_POISONOUS;
}

/**
 * Determine the 'effective' chunk type for a given piece of carrion (chunk or
 * corpse), for the player.
 * E.g., ghouls treat rotting and mutagenic chunks as normal chunks, and
 * players with rPois treat poisonous chunks as clean.
 *
 * @param carrion       The actual chunk or corpse.
 * @param innate        Whether to consider to only consider player species,
 *                      rather than items, forms, mutations, etc (for rPois).
 * @return              A chunk type corresponding to the effect eating the
 *                      given item will have on the player.
 */
corpse_effect_type determine_chunk_effect(const item_def &carrion,
                                          bool innate_only)
{
    return determine_chunk_effect(mons_corpse_effect(carrion.mon_type),
                                                     innate_only);
}

/**
 * Determine the 'effective' chunk type for a given input for the player.
 * E.g., ghouls treat rotting and mutagenic chunks as normal chunks, and
 * players with rPois treat poisonous chunks as clean.
 *
 * @param chunktype     The actual chunk type.
 * @param innate        Whether to consider to only consider player species,
 *                      rather than items, forms, mutations, etc (for rPois).
 * @return              A chunk type corresponding to the effect eating a chunk
 *                      of the given type will have on the player.
 */
corpse_effect_type determine_chunk_effect(corpse_effect_type chunktype,
                                          bool innate_only)
{
    // Determine the initial effect of eating a particular chunk. {dlb}
    switch (chunktype)
    {
    case CE_ROT:
    case CE_MUTAGEN:
        if (you.species == SP_GHOUL)
            chunktype = CE_CLEAN;
        break;

    case CE_POISONOUS:
        // XXX: find somewhere else to pull the species list from
        if (you.species == SP_GHOUL
            || you.species == SP_GARGOYLE
            || !innate_only && player_res_poison(false) > 0)
        {
            chunktype = CE_CLEAN;
        }
        break;

    default:
        break;
    }

    return chunktype;
}

static bool _vampire_consume_corpse(int slot, bool invent)
{
    ASSERT(you.species == SP_VAMPIRE);

    item_def &corpse = (invent ? you.inv[slot]
                               : mitm[slot]);

    ASSERT(corpse.base_type == OBJ_CORPSES);
    ASSERT(corpse.sub_type == CORPSE_BODY);

    if (!mons_has_blood(corpse.mon_type))
    {
        mpr("There is no blood in this body!");
        return false;
    }

    // The delay for eating a chunk (mass 1000) is 2
    // Here the base nutrition value equals that of chunks,
    // but the delay should be smaller.
    const int max_chunks = get_max_corpse_chunks(corpse.mon_type);
    int duration = 1 + max_chunks / 3;
    duration = stepdown_value(duration, 6, 6, 12, 12);

    // Get some nutrition right away, in case we're interrupted.
    // (-1 for the starting message.)
    vampire_nutrition_per_turn(corpse, -1);

    // The draining delay doesn't have a start action, and we only need
    // the continue/finish messages if it takes longer than 1 turn.
    start_delay(DELAY_FEED_VAMPIRE, duration, invent, slot);

    return true;
}

static void _heal_from_food(int hp_amt, bool unrot)
{
    if (hp_amt > 0)
        inc_hp(hp_amt);

    if (unrot && player_rotted())
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

    // Vampires can never starve to death.  Ghouls will just rot much faster.
    if (you.undead_state() != US_ALIVE)
        return (HUNGER_FAINTING + HUNGER_STARVING) / 2; // midpoint

    return 0;
}

void handle_starvation()
{
    if (!you_foodless() && !you.duration[DUR_DEATHS_DOOR]
        && you.hunger <= HUNGER_FAINTING)
    {
        if (!you.cannot_act() && one_chance_in(40))
        {
            mprf(MSGCH_FOOD, "You lose consciousness!");
            stop_running();

            you.increase_duration(DUR_PARALYSIS, 5 + random2(8), 13);
            if (you_worship(GOD_XOM))
                xom_is_stimulated(get_tension() > 0 ? 200 : 100);
        }

        if (you.hunger <= 0)
        {
            mprf(MSGCH_FOOD, "You have starved to death.");
            ouch(INSTANT_DEATH, KILLED_BY_STARVATION);
            if (!you.dead) // if we're still here...
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
        return make_stringf("%d", hunger);
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

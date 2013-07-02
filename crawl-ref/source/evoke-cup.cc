/**
 * @file
 * @brief Cup of Charity evokable item
**/

#include "AppHdr.h"

#include "evoke-cup.h"

#include "food.h"
#include "libutil.h"
#include "player.h"
#include "random.h"
#include "shopping.h"
#include "stuff.h"

#include <sstream>

bool cup_of_charity(actor* evoker, item_def &cup)
{
    const cup_of_charity_effect effect = get_charity_effect(cup);
    const int cost = get_charity_price(cup);

    if (you.duration[DUR_CHARITY])
    {
        mpr("You are already feeling very charitable.");
        return false;
    }

    if (!you_can_drink())
    {
        mpr("You must be able to drink to use this item.");
        return false;
    }

    // Make specific checks for the current effect
    switch (effect)
    {
    case CCHA_SUPER_HEAL:
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("You can't heal while in Death's Door.");
            return false;
        }
        break;
    default:
        break;
    }

    string prompt = make_stringf("Pay a donation of %d gold for %s?", cost,
                                 cup_of_charity_effect_description(cup).c_str());
    if (!yesno(prompt.c_str()))
        return false;

    if (you.gold < cost)
    {
        mprf("Sorry, you cannot afford a donation of this size.");
        return false;
    }

    you.del_gold(cost);

    int dur = 5 + random2avg(you.skill(SK_EVOCATIONS), 3);

    // Perform the effect
    const float mul = cup.props["charity_mul"].get_float();
    switch (effect)
    {
    case CCHA_MULTIPLY_DAMAGE:
        // XXX: At 4x damage, use Quad Damage duration instead
        you.props["damage_mul"].get_float() = mul;
        mpr("You feel very damaging!");
        break;
    case CCHA_SUPER_HEAL:
        {
            // XXX: Super heal should probably cure some bad statuses too.
            // Maybe occasionally cure a bad mutation?

            // Same formula as healing pot
            int factor = (you.species == SP_VAMPIRE
                    && you.hunger_state < HS_SATIATED
                    ? 2 : 1);

            // Unrot some HP
            unrot_hp((2 + random2avg(5 * mul, 2)) / factor);
            // Restore HP
            inc_hp((10 + random2avg(28 * mul, 3)) / factor);

            mpr("You feel much better.");
        }
        break;
    default:
        break;
    }

    you.set_duration(DUR_CHARITY, dur);

    cup.props["charity_uses"] = cup.props["charity_uses"].get_int() + 1;
    if (evoker)
        evoker->props["charity_effect"].get_int() = effect;
    // Re-initialize
    cup_of_charity_init(cup);
    return true;
}

// Timeout effects where needed
void cup_of_charity_ended(actor &evoker)
{
    ASSERT(evoker.props.exists("charity_effect"));
    const int effect_no = evoker.props["charity_effect"].get_int();
    ASSERT(effect_no > CCHA_NO_EFFECT && effect_no <= NUM_CHARITY_EFFECTS);

    cup_of_charity_effect effect = (cup_of_charity_effect)effect_no;
    switch (effect)
    {
    case CCHA_MULTIPLY_DAMAGE:
        you.props.erase("damage_mul");
        break;
    default:
        break;
    }
}

string cup_of_charity_description(const item_def &cup)
{
    ostringstream s;

    const int gold = get_charity_price(cup);
    s << "\n\n" << "Cost:    " << make_stringf("%d gold", gold) << "\n";
    s           << "Effect:  " << cup_of_charity_effect_description(cup) << "\n";
    return s.str();
}

string cup_of_charity_effect_description(const item_def &cup)
{
    ostringstream s;
    const cup_of_charity_effect effect = get_charity_effect(cup);
    const float mul = cup.props["charity_mul"].get_float();
    switch (effect)
    {
    case CCHA_SUPER_HEAL:
        s << make_stringf("%.1fx healing", mul);
        break;
    case CCHA_MULTIPLY_DAMAGE:
        s << make_stringf("+%d%% damage", (int)(mul * 100.0 - 100));
        break;
    default:
        s << "a buggy effect";
        break;
    }
    return s.str();
}


void cup_of_charity_init(item_def &cup)
{
    const cup_of_charity_effect effect =
        (cup_of_charity_effect)(random2(NUM_CHARITY_EFFECTS) + 1);
    cup.plus = effect;

    // Set price
    int num_uses = 0;
    if (cup.props.exists("charity_uses"))
        num_uses = cup.props["charity_uses"].get_int();

    cup.props["charity_uses"] = num_uses;
    int price = 3 + num_uses * 2;
    price *= price;

    // Get a strength scale 0..100
    int scale = max(0, price / 100);

    // Finish deciding the effects and modifying the price
    double mul = 1.0;
    switch (effect)
    {
    case CCHA_SUPER_HEAL:
        {
            // 1x .. 4x
            const int mul_max = 3 + div_rand_round(scale, 20);
            mul = ((float)random_range(2, mul_max)) / 2.0;
        }
        break;
    case CCHA_MULTIPLY_DAMAGE:
        {
            // 1.25x .. 4x
            const int mul_max = 6 + div_rand_round(scale, 10);
            mul = ((float)random_range(5, mul_max)) / 4.0;
        }
        break;
    default:
        break;
    }
    if (mul > 1.0)
    {
        // Notch the price up based on the multiplier
        price += ((float)price) * ((mul - 1.0) / 5.0);
    }
    cup.props["charity_mul"].get_float() = mul;

    // Further adjust the price by +/- 10%
    price *= div_rand_round(random_range(90, 110), 100);
    cup.props["charity_price"].get_int() = price;
}

cup_of_charity_effect get_charity_effect(const item_def &cup)
{
    ASSERT(cup.plus > CCHA_NO_EFFECT && cup.plus <= NUM_CHARITY_EFFECTS);
    return (cup_of_charity_effect)(cup.plus);
}

int get_charity_price(const item_def &cup)
{
    ASSERT(cup.props.exists("charity_price"));
    int cost = cup.props["charity_price"].get_int();
    ASSERT(cost > 0);

    // Evocations discount
    cost *= (100 - you.skill(SK_EVOCATIONS));
    cost /= 100;

    // Apply bargain card
    if (you.duration[DUR_BARGAIN])
        cost = bargain_cost(cost);

    return cost;
}

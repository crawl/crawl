/**
 * @file
 * @brief Cup of Charity evokable item
**/

#ifndef EVOKECUP_H
#define EVOKECUP_H

enum cup_of_charity_effect
{
    CCHA_NO_EFFECT,
    CCHA_MULTIPLY_DAMAGE,
    CCHA_SUPER_HEAL,
    NUM_CHARITY_EFFECTS = CCHA_SUPER_HEAL
};

bool cup_of_charity(actor* evoker, item_def &cup);
cup_of_charity_effect get_charity_effect(const item_def &cup);
int get_charity_price(const item_def &cup);
string cup_of_charity_description(const item_def &cup);
string cup_of_charity_effect_description(const item_def &cup);
void cup_of_charity_init(item_def &cup);
void cup_of_charity_ended(actor &evoker);

#endif

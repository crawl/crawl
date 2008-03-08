/*
 *  File:       it_use2.h
 *  Summary:    Functions for using wands, potions, and weapon/armour removal.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */

#ifndef IT_USE2_H
#define IT_USE2_H


#include "externs.h"

enum potion_type
{
    POT_HEALING,                       //    0
    POT_HEAL_WOUNDS,
    POT_SPEED,
    POT_MIGHT,
    POT_GAIN_STRENGTH,
    POT_GAIN_DEXTERITY,                //    5
    POT_GAIN_INTELLIGENCE,
    POT_LEVITATION,
    POT_POISON,
    POT_SLOWING,
    POT_PARALYSIS,                     //   10
    POT_CONFUSION,
    POT_INVISIBILITY,
    POT_PORRIDGE,
    POT_DEGENERATION,
    POT_DECAY,                         //   15
    POT_WATER,
    POT_EXPERIENCE,
    POT_MAGIC,
    POT_RESTORE_ABILITIES,
    POT_STRONG_POISON,                 //   20
    POT_BERSERK_RAGE,
    POT_CURE_MUTATION,
    POT_MUTATION,
    POT_BLOOD,
    POT_RESISTANCE,
    NUM_POTIONS
};

/* ***********************************************************************
 * called from: ability - beam - decks - item_use - misc - religion -
 *              spell - spells - spells1
 * *********************************************************************** */
bool potion_effect(potion_type pot_eff, int pow);


/* ***********************************************************************
 * called from: item_use
 * *********************************************************************** */
void unuse_randart(unsigned char unw);

void unuse_randart(const item_def &item);

/* ***********************************************************************
 * called from: item_use - transfor
 * *********************************************************************** */
void unwear_armour(char unw);


/* ***********************************************************************
 * called from: decks - it_use3 - item_use - items - spells3 - transfor
 * *********************************************************************** */
bool unwield_item(bool showMsgs = true);

#endif

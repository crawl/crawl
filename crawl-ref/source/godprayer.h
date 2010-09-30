/*
 * File:     godprayer.h
 * Summary:  Prayer and sacrifice.
 */

#ifndef GODPRAYER_H
#define GODPRAYER_H

#include "religion-enum.h"

std::string god_prayer_reaction();
bool god_accepts_prayer(god_type god);
void pray();
void end_prayer();

piety_gain_t sacrifice_item_stack(const item_def& item);
void offer_items();

#endif

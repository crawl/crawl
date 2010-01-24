/*
 * File:     godprayer.h
 * Summary:  Prayer and sacrifice.
 */

#ifndef GODPRAYER_H
#define GODPRAYER_H

std::string god_prayer_reaction();
bool god_accepts_prayer(god_type god);
void pray();
void end_prayer();
void offer_items();

#endif


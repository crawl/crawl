#pragma once

#include <vector>
#include "monster.h"

using std::vector;

string describe_monsters_condensed(const vector<monster*>& monsters);

void update_monsters_in_view();
void maybe_notice_monster(monster& mons, bool stepped = false);
void notice_new_monsters(vector<monster*>& monsters, vector<monster*>& to_announce,
                         seen_context_type sc = SC_NONE);
void monster_encounter_message(monster& mon);

void queue_monster_announcement(monster& mons, seen_context_type sc);
void notice_queued_monsters();

void seen_monster(monster* mons, bool do_encounter_message = true);

string multimonster_name_string(vector<monster*> monsters);

/**
 * @file
 * @brief Acquirement and Trog/Oka/Sif gifts.
**/

#pragma once

#include "item-prop-enum.h"

bool acquirement_menu();

bool okawaru_gift_weapon();
bool okawaru_gift_armour();

void make_acquirement_items();

int acquirement_create_item(object_class_type class_wanted, int agent,
                            bool quiet, const coord_def &pos = coord_def());

vector<object_class_type> shuffled_acquirement_classes(bool scroll);

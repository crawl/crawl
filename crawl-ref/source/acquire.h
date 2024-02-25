/**
 * @file
 * @brief Acquirement and Trog/Oka/Sif gifts.
**/

#pragma once

#include "item-prop-enum.h"

#define INVENT_GIZMO_USED_KEY "invent_gizmo_used"
#define COGLIN_GIZMO_KEY "coglin_gizmos"
#define COGLIN_GIZMO_NAMES_KEY "coglin_gizmo_names"

const int COGLIN_GIZMO_NUM = 3;
const int COGLIN_GIZMO_XL = 14;

bool acquirement_menu();

bool okawaru_gift_weapon();
bool okawaru_gift_armour();

bool coglin_invent_gizmo();
void coglin_announce_gizmo_name();

void make_acquirement_items();

int acquirement_create_item(object_class_type class_wanted, int agent,
                            bool quiet, const coord_def &pos = coord_def());

vector<object_class_type> shuffled_acquirement_classes(bool scroll);

/**
 * @file
 * @brief Acquirement and Trog/Oka/Sif gifts.
**/

#pragma once

#include "coord-def.h"
#include "item-prop-enum.h"
#include "object-class-type.h"

bool acquirement_menu();

int acquirement_create_item(object_class_type class_wanted, int agent,
                            bool quiet, const coord_def &pos = coord_def());

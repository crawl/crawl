/**
 * @file
 * @brief Acquirement and Trog/Oka/Sif gifts.
**/

#pragma once

#include "item-prop-enum.h"

bool acquirement(object_class_type force_class, int agent,
                 bool quiet = false, int *item_index = nullptr,
                 bool known_scroll = false);

int acquirement_create_item(object_class_type class_wanted,
                            int agent, bool quiet,
                            const coord_def &pos);

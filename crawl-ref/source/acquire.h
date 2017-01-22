/**
 * @file
 * @brief Acquirement and Trog/Oka/Sif gifts.
**/

#ifndef ACQUIRE_H
#define ACQUIRE_H

bool acquirement(object_class_type force_class, int agent,
                 bool quiet = false, int *item_index = nullptr,
                 bool debug = false);

int acquirement_create_item(object_class_type class_wanted,
                            int agent, bool quiet,
                            const coord_def &pos, bool debug = false);
int spell_weight(spell_type spell);
#endif

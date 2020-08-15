/**
 * @file
 * @brief Acquirement and Trog/Oka/Sif gifts.
**/

#pragma once

bool acquirement_menu();
bool artefact_acquirement_menu();

int acquirement_create_item(object_class_type class_wanted, int agent,
    bool quiet, const coord_def& pos = coord_def());

int acquirement_artefact(object_class_type class_wanted,
    bool randart, const coord_def& pos = coord_def());

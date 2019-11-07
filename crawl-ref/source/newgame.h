/**
 * @file
 * @brief Functions used when starting a new game.
**/

#pragma once

#include "job-type.h"
#include "species-type.h"

class UINewGameMenu;
struct menu_letter;
struct newgame_def;

string newgame_char_description(const newgame_def& ng);

void choose_tutorial_character(newgame_def& ng_choice);

bool choose_game(newgame_def& ng, newgame_def& choice,
                 const newgame_def& defaults);

string newgame_random_name();

/*
 * A structure for grouping backgrounds by category.
 */
struct job_group
{
    const char* name;   ///< Name of the group.
    coord_def position; ///< Relative coordinates of the title.
    int width;          ///< Column width.
    vector<job_type> jobs; ///< List of jobs in the group.

    /// A method to attach the group to a freeform.
    void attach(const newgame_def& ng, const newgame_def& defaults,
                UINewGameMenu* menu, menu_letter &letter);
};

struct species_group
{
    const char* name;   ///< Name of the group.
    coord_def position; ///< Relative coordinates of the title.
    int width;          ///< Column width.
    vector<species_type> species_list; ///< List of species in the group.

    /// A method to attach the group to a freeform.
    void attach(const newgame_def& ng, const newgame_def& defaults,
                UINewGameMenu* menu, menu_letter &letter);
};

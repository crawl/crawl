/**
 * @file
 * @brief Collection of startup related functions and objects
**/

#pragma once

struct newgame_def;

void startup_initialize();
newgame_def startup_step(const newgame_def& defaults);
bool startup_load_regular(newgame_def choice, const newgame_def& defaults);
void cio_init();

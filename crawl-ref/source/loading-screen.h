/**
 * @file
 * @brief Loading splash screen UI layout.
**/

#pragma once

#ifdef USE_TILE_LOCAL

#include <string>

void loading_screen_open();
void loading_screen_close();
void loading_screen_update_msg(string message);

#endif

/**
 * @file
 * @brief Wizard mode command handling.
**/

#pragma once

#ifdef WIZARD
int list_wizard_commands(bool do_redraw_screen = false);
void handle_wizard_command();
void enter_explore_mode();
#endif

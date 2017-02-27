/**
 * @file
 * @brief Prompts.
 **/

#pragma once

bool yes_or_no(PRINTF(0, ));
typedef map<int, int> explicit_keymap;
bool yesno(const char * str, bool allow_lowercase, int default_answer,
           bool clear_after = true, bool interrupt_delays = true,
           bool noprompt = false,
           const explicit_keymap *map = nullptr,
           GotoRegion = GOTO_MSG);

int yesnoquit(const char* str, bool safe = true, int default_answer = 0,
              bool allow_all = false, bool clear_after = true,
              char alt_yes = 'Y', char alt_yes2 = 'Y');

int prompt_for_quantity(const char *prompt);
int prompt_for_int(const char *prompt, bool nonneg);
double prompt_for_float(const char* prompt);

char index_to_letter(int the_index);

int letter_to_index(int the_letter);


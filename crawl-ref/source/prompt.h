/**
 * @file
 * @brief Prompts.
 **/

#pragma once

bool yes_or_no(PRINTF(0, ));
typedef map<int, int> explicit_keymap;
int yesno(const char * str, bool allow_lowercase, int default_answer,
          bool clear_after = true, bool interrupt_delays = true,
          bool noprompt = false,
          const explicit_keymap *map = nullptr,
          bool allow_popup = true,
          bool ask_always = false);

int prompt_for_quantity(const char *prompt);
int prompt_for_int(const char *prompt, bool nonneg, const string &prefill = "");
double prompt_for_float(const char* prompt);

char index_to_letter(int the_index);

int letter_to_index(int the_letter);

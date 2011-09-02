/**
 * @file
 * @brief Misc stuff.
**/


#ifndef STUFF_H
#define STUFF_H

#include <map>

std::string make_time_string(time_t abs_time, bool terse = false);
std::string make_file_time(time_t when);

void set_redraw_status(uint64_t flags);

int stepdown_value(int base_value, int stepping, int first_step,
                   int last_step, int ceiling_value);
unsigned char get_ch();

void cio_init();
void cio_cleanup();
void clear_globals_on_exit();
NORETURN void end(int exit_code, bool print_err = false,
         const char *format = NULL, ...);
NORETURN void game_ended();
NORETURN void game_ended_with_error(const std::string &message);

bool print_error_screen(const char *message, ...);
void redraw_screen();

void canned_msg(canned_message_type which_message);

bool yes_or_no(const char* fmt, ...);
typedef std::map<int, int> explicit_keymap;
bool yesno(const char * str, bool safe = true, int safeanswer = 0,
            bool clear_after = true, bool interrupt_delays = true,
            bool noprompt = false,
            const explicit_keymap *map = NULL,
            GotoRegion = GOTO_MSG);

int yesnoquit(const char* str, bool safe = true, int safeanswer = 0,
               bool allow_all = false, bool clear_after = true,
               char alt_yes = 'Y', char alt_yes2 = 'Y');

char index_to_letter(int the_index);

int letter_to_index(int the_letter);

maybe_bool frombool(bool b);
bool tobool(maybe_bool mb, bool def);
bool tobool(maybe_bool mb);

class game_ended_condition : public std::exception
{
};

int prompt_for_quantity(const char *prompt);
int prompt_for_int(const char *prompt, bool nonneg);

#endif

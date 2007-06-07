#ifndef __LIBDOS_H__
#define __LIBDOS_H__

void init_libdos();

int get_number_of_lines();
int get_number_of_cols();

inline void enable_smart_cursor(bool ) { }
inline bool is_smart_cursor_enabled()  { return (false); }
void set_cursor_enabled(bool enabled);
bool is_cursor_enabled();
void clear_to_end_of_line();

void message_out(int mline, int colour, const char *str, int firstcol = 0,
                 bool newline = true);
void clear_message_window();
inline void update_screen()
{
}

inline void putwch(unsigned c)
{
    putch(static_cast<char>(c));
}

#endif

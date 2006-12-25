#ifndef __LIBDOS_H__
#define __LIBDOS_H__

void init_libdos();

inline void enable_smart_cursor(bool ) { }
inline bool is_smart_cursor_enabled()  { return (false); }
void set_cursor_enabled(bool enabled);
bool is_cursor_enabled();
void clear_to_end_of_line();

#endif

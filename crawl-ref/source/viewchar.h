#ifndef VIEWCHAR_H
#define VIEWCHAR_H

void init_char_table(char_set_type set);

dungeon_char_type get_feature_dchar(dungeon_feature_type feat);
unsigned dchar_glyph(dungeon_char_type dchar);

std::string stringize_glyph(ucs_t glyph);

dungeon_char_type dchar_by_name(const std::string &name);

#endif

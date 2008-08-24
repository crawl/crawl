#ifndef TILE_LIST_PROCESSOR_H
#define TILE_LIST_PROCESSOR_H

#include "tile.h"
#include "tile_page.h"
#include <string>
#include <vector>

class tile_list_processor
{
public:
    tile_list_processor();
    virtual ~tile_list_processor();

    bool process_list(const char *list_file);
    bool write_data();
protected:
    bool load_image(tile &img, const char *filename);
    bool process_line(char *read_line, const char *list_file, int line);
    void add_image(tile &img, const char *enumname);

    std::string m_name;

    tile_page m_page;
    unsigned int m_last_enum;

    // image options
    bool m_rim;
    bool m_corpsify;
    bool m_composing;
    bool m_shrink;
    std::vector<tile*> m_back;
    std::string m_parts_ctg;
    std::string m_sdir;
    std::string m_prefix;
    std::vector<std::string> m_categories;
    std::vector<int> m_ctg_counts;
    tile m_compose;
};

#endif

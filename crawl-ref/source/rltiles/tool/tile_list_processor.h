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
    void recolour(tile &img);

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
    std::string m_start_value;
    std::string m_include;
    std::vector<std::string> m_categories;
    std::vector<int> m_ctg_counts;
    tile m_compose;
    int m_variation_idx;
    int m_variation_col;

    typedef std::pair<tile_colour, tile_colour> palette_entry;
    typedef std::vector<palette_entry> palette_list;
    palette_list m_palette;

    typedef std::pair<int, int> int_pair;
    typedef std::vector<int_pair> hue_list;
    hue_list m_hues;

    typedef std::vector<int> desat_list;
    desat_list m_desat;

    typedef std::vector<int_pair> lum_list;
    lum_list m_lum;
};

#endif

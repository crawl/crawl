#ifndef TILE_LIST_PROCESSOR_H
#define TILE_LIST_PROCESSOR_H

#include "tile.h"
#include "tile_page.h"
#include <string>
#include <vector>
#include <stdio.h>

class tile_list_processor
{
public:
    tile_list_processor();
    virtual ~tile_list_processor();

    bool process_list(const char *list_file);
    bool write_data(bool image, bool code);
protected:
    bool load_image(tile &img, const char *filename, bool background = false);
    bool process_line(char *read_line, const char *list_file, int line);
    void add_image(tile &img, const char *enumname);
    void recolour(tile &img);

    void add_abstracts(
        FILE *fp,
        const char *format,
        const vector<string> &lc_enum,
        const vector<string> &uc_max_enum,
        bool is_js = false);

    string m_name;

    tile_page m_page;
    unsigned int m_last_enum;

    // image options
    bool m_rim;
    bool m_corpsify;
    bool m_composing;
    bool m_shrink;
    vector<tile*> m_back;
    string m_parts_ctg;
    string m_sdir;
    string m_back_sdir;
    string m_prefix;
    string m_start_value;
    string m_start_value_module;
    vector<string> m_include;
    vector<string> m_categories;
    vector<int> m_ctg_counts;
    tile m_compose;
    tile* m_texture;
    int m_variation_idx;
    int m_variation_col;
    int m_weight;
    double m_alpha;
    int m_domino;

    typedef pair<tile_colour, tile_colour> palette_entry;
    typedef vector<palette_entry> palette_list;
    palette_list m_palette;

    typedef pair<int, int> int_pair;
    typedef vector<int_pair> hue_list;
    hue_list m_hues;

    typedef vector<int> desat_list;
    desat_list m_desat;

    typedef vector<int_pair> lum_list;
    lum_list m_lum;

    vector<string> m_depends;

    typedef pair<string, string> string_pair;
    typedef vector<string_pair> string_pair_list;
    string_pair_list m_abstract;
};

#endif

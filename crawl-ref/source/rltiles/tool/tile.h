#ifndef TILE_H
#define TILE_H

#include "tile_colour.h"
#include <string>

class tile
{
public:
    tile();
    tile(const tile &img, const char *enumnam = NULL, const char *parts = NULL);
    virtual ~tile();

    bool load(const std::string &new_filename);
    bool load(const std::string &new_filename, const std::string &new_enumname);

    void unload();
    bool valid() const;

    void resize(unsigned int new_width, unsigned int new_height);

    void add_rim(const tile_colour &rim);
    void corpsify();
    void corpsify(unsigned int corpse_width, unsigned int corpse_height,
                  unsigned int cut_separate, unsigned int cut_height,
                  const tile_colour &wound);

    void copy(const tile &img);
    bool compose(const tile &img);

    void replace_colour(tile_colour &find, tile_colour &replace);
    void fill(const tile_colour &col);

    const std::string &filename();
    const std::string &enumname();
    const std::string &parts_ctg();
    int width();
    int height();
    bool shrink();
    void set_shrink(bool new_shrink);

    void get_bounding_box(unsigned int &x0, unsigned int &y0,
                          unsigned int &w, unsigned int &h);

    tile_colour &get_pixel(unsigned int x, unsigned int y);
protected:
    unsigned int m_width;
    unsigned int m_height;
    std::string m_filename;
    std::string m_enumname;
    std::string m_parts_ctg;
    tile_colour *m_pixels;
    bool m_shrink;
};

#endif

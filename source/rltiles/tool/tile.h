#ifndef TILE_H
#define TILE_H

#include "tile_colour.h"
#include <string>
#include <vector>
using namespace std;

class tile
{
public:
    tile();
    tile(const tile &img, const char *enumnam = NULL, const char *parts = NULL);
    virtual ~tile();

    bool load(const string &new_filename);
    bool load(const string &new_filename, const string &new_enumname);

    void unload();
    bool valid() const;

    void resize(int new_width, int new_height);

    void add_rim(const tile_colour &rim);
    void corpsify();
    void corpsify(int corpse_width, int corpse_height,
                  int cut_separate, int cut_height, const tile_colour &wound);

    void copy(const tile &img);
    bool compose(const tile &img);
    bool texture(const tile &img);

    void replace_colour(tile_colour &find, tile_colour &replace);
    void fill(const tile_colour &col);

    const string &filename() const;
    int enumcount() const;
    const string &enumname(int idx) const;
    void add_enumname(const string &name);
    const string &parts_ctg() const;
    int width() const;
    int height() const;
    bool shrink();
    void set_shrink(bool new_shrink);

    void get_bounding_box(int &x0, int &y0, int &w, int &h);

    tile_colour &get_pixel(int x, int y);

    void add_variation(int colour, int idx);
    bool get_variation(int colour, int &idx);
protected:
    int m_width;
    int m_height;
    string m_filename;
    vector<string> m_enumname;
    string m_parts_ctg;
    tile_colour *m_pixels;
    bool m_shrink;

    int m_variations[MAX_COLOUR];
};

#endif

#ifndef TILE_PAGE_H
#define TILE_PAGE_H

#include <vector>
class tile;

class tile_page
{
public:
    tile_page();
    virtual ~tile_page();

    bool place_images();
    bool write_image(const char *filename);

    std::vector<tile*> m_tiles;
    std::vector<unsigned int> m_counts;
    std::vector<int> m_texcoords;
    std::vector<int> m_offsets;
protected:
    int m_width;
    int m_height;
};

#endif

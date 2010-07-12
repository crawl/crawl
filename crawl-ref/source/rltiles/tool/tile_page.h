#ifndef TILE_PAGE_H
#define TILE_PAGE_H

#include <vector>
#include <string>

class tile;

class tile_page
{
public:
    tile_page();
    virtual ~tile_page();

    bool place_images();
    bool write_image(const char *filename);

    int find(const std::string &enumname) const;
    bool add_synonym(const std::string &enumname, const std::string &syn);
    bool add_synonym(int idx, const std::string &syn);
    void add_variation(int var_idx, int base_idx, int colour);

    std::vector<tile*> m_tiles;
    std::vector<unsigned int> m_counts;
    std::vector<int> m_texcoords;
    std::vector<int> m_offsets;
    std::vector<unsigned int> m_probs;
    std::vector<unsigned int> m_base_tiles;

protected:
    int m_width;
    int m_height;
};

#endif

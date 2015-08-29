#ifndef TILE_PAGE_H
#define TILE_PAGE_H

#include <vector>
#include <string>
using namespace std;

class tile;

class tile_page
{
public:
    tile_page();
    virtual ~tile_page();

    bool place_images();
    bool write_image(const char *filename);

    int find(const string &enumname) const;
    bool add_synonym(const string &enumname, const string &syn);
    bool add_synonym(int idx, const string &syn);
    void add_variation(int var_idx, int base_idx, int colour);

    vector<tile*> m_tiles;
    vector<unsigned int> m_counts;
    vector<int> m_texcoords;
    vector<int> m_offsets;
    vector<unsigned int> m_probs;
    vector<unsigned int> m_base_tiles;
    vector<unsigned int> m_domino;

protected:
    int m_width;
    int m_height;
};

#endif

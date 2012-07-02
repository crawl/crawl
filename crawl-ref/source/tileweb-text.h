#ifdef USE_TILE_WEB
#ifndef TILEWEB_TEXT_H
#define TILEWEB_TEXT_H

#include <string>

class WebTextArea
{
public:
    WebTextArea(std::string name);
    virtual ~WebTextArea();

    void resize(int mx, int my);

    void clear();

    void set_character(ucs_t chr, int fg, int bg, int x, int y);
    void append_character(ucs_t chr, int x, int y);
    void clear_cell(int x, int y);

    void send(bool force = false);

    int mx, my; // Size

protected:
    std::wstring  *m_cbuf; /* Character buffer. Cells can contain 0 or more than
                              one code point with CJK and combining characters,
                              thus the wstring.
                            */
    uint8_t       *m_abuf; // Color buffer

    std::wstring  *m_old_cbuf;
    uint8_t       *m_old_abuf;

    std::string m_client_side_name;

    bool m_dirty;

    virtual void on_resize();
};

#endif
#endif

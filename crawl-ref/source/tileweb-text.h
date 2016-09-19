#ifdef USE_TILE_WEB
#ifndef TILEWEB_TEXT_H
#define TILEWEB_TEXT_H

#include <string>

class WebTextArea
{
public:
    WebTextArea(string name);
    virtual ~WebTextArea();

    void resize(int mx, int my);

    void clear();

    void put_character(char32_t chr, int fg, int bg, int x, int y);

    void send(bool force = false);

    int mx, my; // Size

protected:
    char32_t *m_cbuf; // Character buffer
    uint8_t *m_abuf; // Color buffer

    char32_t *m_old_cbuf;
    uint8_t *m_old_abuf;

    string m_client_side_name;

    bool m_dirty;

    virtual void on_resize();
};

#endif
#endif


#include "AppHdr.h"

#include "tileweb-text.h"
#include "tileweb.h"

#include <sstream>

WebTextArea::WebTextArea(std::string name) :
    mx(0),
    my(0),
    m_cbuf(NULL),
    m_abuf(NULL),
    m_old_cbuf(NULL),
    m_old_abuf(NULL),
    m_client_side_name(name),
    m_dirty(true)
{
}

WebTextArea::~WebTextArea()
{
    if (m_cbuf != NULL)
    {
        delete[] m_cbuf;
        delete[] m_abuf;
        delete[] m_old_cbuf;
        delete[] m_old_abuf;
    }
}

void WebTextArea::resize(int x, int y)
{
    if (mx == x && my == y)
        return;

    mx = x;
    my = y;

    if (m_cbuf != NULL)
    {
        delete[] m_cbuf;
        delete[] m_abuf;
        delete[] m_old_cbuf;
        delete[] m_old_abuf;
    }

    int size = mx * my;
    m_cbuf = new ucs_t[size];
    m_abuf = new uint8_t[size];
    m_old_cbuf = new ucs_t[size];
    m_old_abuf = new uint8_t[size];

    for (int i = 0; i < mx * my; i++)
    {
        m_cbuf[i] = ' ';
        m_abuf[i] = 0;
        m_old_cbuf[i] = ' ';
        m_old_abuf[i] = 0;
    }

    m_dirty = true;

    on_resize();
}

void WebTextArea::clear()
{
    for (int i = 0; i < mx * my; i++)
    {
        m_cbuf[i] = ' ';
        m_abuf[i] = 0;
    }

    m_dirty = true;
}

void WebTextArea::put_character(ucs_t chr, int fg, int bg, int x, int y)
{
    ASSERT((x < mx) && (y < my) && (x >= 0) && (y >= 0));
    uint8_t col = (fg & 0xf) + (bg << 4);

    if ((m_cbuf[x + y * mx] != chr) || (m_abuf[x + y * mx] != col))
        m_dirty = true;

    m_cbuf[x + y * mx] = chr;
    m_abuf[x + y * mx] = col;
}

void WebTextArea::send(bool force)
{
    if (m_cbuf == NULL) return;
    if (!force && !m_dirty) return;
    m_dirty = false;

    bool start = true;
    uint8_t last_col = 0;
    int space_count = 0;
    bool dirty = false;
    std::wstringstream html;

    bool sending = false;

    for (int y = 0; y < my; ++y)
    {
        start = true;
        space_count = 0;
        dirty = false;
        html.str(L"");

        for (int x = 0; x < mx; ++x)
        {
            ucs_t chr = m_cbuf[x + y * mx];
            uint8_t col = m_abuf[x + y * mx];

            if (chr != m_old_cbuf[x + y * mx] ||
                col != m_old_abuf[x + y * mx])
            {
                dirty = true;
                m_old_cbuf[x + y * mx] = chr;
                m_old_abuf[x + y * mx] = col;
            }

            if (chr != ' ' || ((col >> 4) & 0xF) != 0)
            {
                while (space_count)
                {
                    html << "&nbsp;";
                    space_count--;
                }
            }

            if ((col != last_col) && !start)
                html << "</span>";
            if ((col != last_col) || start)
                html << "<span class=\\\"fg" << (col & 0xf)
                     << " bg" << ((col >> 4) & 0xf) << "\\\">";
            last_col = col;
            start = false;

            if (chr == ' ' && ((col >> 4) & 0xF) == 0)
                space_count++;
            else
            {
                switch (chr)
                {
                case ' ':
                    html << "&nbsp;";
                    break;
                case '<':
                    html << "&lt;";
                    break;
                case '>':
                    html << "&gt;";
                    break;
                case '&':
                    html << "&amp;";
                    break;
                case '\\':
                    html << "\\\\";
                    break;
                case '"':
                    html << "&quot;";
                    break;
                default:
                    html.put(chr);
                    break;
                }
            }
        }

        if (dirty || force)
        {
            if (!sending)
            {
                tiles.write_message("txt('%s',{", m_client_side_name.c_str());
                sending = true;
            }

            tiles.write_message("%u:\"%ls\",", y, html.str().c_str());
        }
    }
    if (sending)
        tiles.send_message("});");
}

void WebTextArea::on_resize()
{
}

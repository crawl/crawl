
#include "AppHdr.h"

#include "tileweb-text.h"

WebTextArea::WebTextArea(std::string name) :
    mx(0),
    my(0),
    m_cbuf(NULL),
    m_abuf(NULL),
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
    }

    int size = mx * my;
    m_cbuf = new ucs_t[size];
    m_abuf = new uint8_t[size];

    clear();

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

    fprintf(stdout, "$(\"#%s\").html(\"", m_client_side_name.c_str());

    uint8_t last_col = 0;
    bool start = true;

    int space_count = 0;

    for (int y = 0; y < my; ++y)
        for (int x = 0; x < mx; ++x)
        {
            ucs_t chr = m_cbuf[x + y * mx];
            uint8_t col = m_abuf[x + y * mx];

            if (chr != ' ' || ((col >> 4) & 0xF) != 0)
            {
                while (space_count)
                {
                    fprintf(stdout, "&nbsp;");
                    space_count--;
                }
            }

            if ((col != last_col) && !start)
                fprintf(stdout, "</span>");
            if ((col != last_col) || start)
                fprintf(stdout, "<span class=\\\"fg%d bg%d\\\">",
                        col & 0xf, (col >> 4) & 0xf);
            last_col = col;
            start = false;

            if (chr == ' ' && ((col >> 4) & 0xF) == 0)
                space_count++;
            else
            {
                switch (chr)
                {
                case ' ':
                    fprintf(stdout, "&nbsp;");
                    break;
                case '<':
                    fprintf(stdout, "&lt;");
                    break;
                case '>':
                    fprintf(stdout, "&gt;");
                    break;
                case '&':
                    fprintf(stdout, "&amp;");
                    break;
                case '\\':
                    fprintf(stdout, "\\\\");
                    break;
                case '"':
                    fprintf(stdout, "&quot;");
                    break;
                default:
                    fprintf(stdout, "%lc", chr);
                    break;
                }
            }

            if (x == mx - 1)
            {
                space_count = 0;
                fprintf(stdout, "<br>");
            }
        }

    fprintf(stdout, "</span>\");\n");
}

void WebTextArea::on_resize()
{
}

CRTTextArea::CRTTextArea() : WebTextArea("crt") { }

void CRTTextArea::on_resize()
{
    WebTextArea::on_resize();
}

StatTextArea::StatTextArea() : WebTextArea("stats") { }

void StatTextArea::on_resize()
{
    WebTextArea::on_resize();
}

MessageTextArea::MessageTextArea() : WebTextArea("messages") { }

void MessageTextArea::on_resize()
{
    WebTextArea::on_resize();
}

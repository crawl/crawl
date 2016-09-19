#include "AppHdr.h"

#ifdef USE_TILE_LOCAL
#ifdef USE_FT

#include "fontwrapper-ft.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "defines.h"
#include "errno.h"
#include "files.h"
#include "format.h"
#include "glwrapper.h"
#include "options.h"
#include "syscalls.h"
#include "tilebuf.h"
#include "tilefont.h"
#include "unicode.h"

// maximum number of unique glyphs that can be rendered with this font at once; e.g. 4096, 256, 36
#define MAX_GLYPHS 256
// dimensions of glyph grid; GLYPHS_PER_ROWCOL^2 <= MAX_GLYPHS; e.g. 64, 16, 6
#define GLYPHS_PER_ROWCOL 16
// char to use if we can't find it in the font (upside-down question mark)
#define MISSING_CHAR 0xbf

#if 0
# define dprintf(...) debuglog(__VA_ARGS__)
#else
# define dprintf(...) (void)0
#endif

FontWrapper* FontWrapper::create()
{
    return new FTFontWrapper();
}

FTFontWrapper::FTFontWrapper() :
    m_glyphs(nullptr),
    m_glyphs_lru(0),
    m_glyphs_mru(0),
    m_glyphs_top(0),  // reinitialised to 1 in load_font
    m_max_advance(0, 0),
    m_min_offset(0),
    charsz(1,1),
    m_max_width(0),
    m_max_height(0)
{
    m_buf = GLShapeBuffer::create(true, true);
}

FTFontWrapper::~FTFontWrapper()
{
    delete[] m_glyphs;
    delete[] pixels;
    delete m_buf;
}

bool FTFontWrapper::load_font(const char *font_name, unsigned int font_size,
                              bool outline, int sc_num, int sc_den)
{
    FT_Library library;
    FT_Error error;

    this->scale_num = sc_num;
    this->scale_den = sc_den;

    outl = outline;

    error = FT_Init_FreeType(&library);
    if (error)
        die_noline("Failed to initialise freetype library.\n");

    // TODO enne - need to find a cross-platform way to also
    // attempt to locate system fonts by name...
    // 1KB: fontconfig if we are not scared of hefty libraries
    string font_path = datafile_path(font_name, false, true);
    if (font_path.c_str()[0] == 0)
        die_noline("Could not find font '%s'\n", font_name);

    // Certain versions of freetype have problems reading files on Windows,
    // do that ourselves.
    FILE *f = fopen_u(font_path.c_str(), "rb");
    if (!f)
        die_noline("Could not read font '%s'\n", font_name);
    unsigned long size = file_size(f);
    FT_Byte *ttf = (FT_Byte*)malloc(size);
    ASSERT(ttf);
    if (fread(ttf, 1, size, f) != size)
        die_noline("Could not read font '%s': %s\n", font_name, strerror(errno));
    fclose(f);
    // FreeType needs the font until FT_Done_Face(), and we never call it.

    error = FT_New_Memory_Face(library, ttf, size, 0, &face);
    if (error == FT_Err_Unknown_File_Format)
        die_noline("Unknown font format for file '%s'\n", font_path.c_str());
    else if (error)
    {
        die_noline("Invalid font from file '%s' (size %lu): 0x%0x\n",
                   font_path.c_str(), size, error);
    }

    error = FT_Set_Pixel_Sizes(face, font_size * scale_num / scale_den,
                                     font_size * scale_num / scale_den);
    ASSERT(!error);

    // Get maximum advance and other global metrics
    FT_Size_Metrics metrics = face->size->metrics;
    m_max_advance   = coord_def(0,0);
    m_max_advance.x = metrics.max_advance >> 6;
    m_max_advance.y = (metrics.ascender-metrics.descender)>>6;
    m_ascender      = (metrics.ascender>>6);
    m_max_width     = (face->bbox.xMax >> 6) - (face->bbox.xMin >> 6);
    m_max_height    = (face->bbox.yMax>>6)-(face->bbox.yMin>>6);//m_max_advance.y;
    m_min_offset    = 0;
    m_glyphs        = new GlyphInfo[MAX_GLYPHS];

    if (outl)
        m_max_width += 2, m_max_height += 2;

    // Grow character size to power of 2
    while (charsz.x < m_max_width)
        charsz.x *= 2;
    while (charsz.y < m_max_height)
        charsz.y *= 2;

    // Fill out texture to be (16*charsz.x) X (16*charsz.y) X (32-bit)
    // Having to blow out 8-bit alpha values into full 32-bit textures is
    // kind of frustrating, but not all OpenGL implementations support the
    // "esoteric" ALPHA8 format and it's not like this texture is very large.

    // [frogbotherer] I think we can get memory usage lower by blowing out
    // the texture as a whole out to a power of 2, instead of each individual
    // character. Also, whilst GLES baulks at ALPHA8, there might be some
    // other compression format that we can use to get the size down a bit
    m_ft_width  = GLYPHS_PER_ROWCOL * charsz.x;
    m_ft_height = GLYPHS_PER_ROWCOL * charsz.y;

    pixels = new unsigned char[4 * charsz.x * charsz.y];
    memset(pixels, 0, sizeof(unsigned char) * 4 * charsz.x * charsz.y);

    dprintf("new font tex %d x %d x 4 = %dpx %d bytes\n",
            m_ft_width, m_ft_height, m_ft_width * m_ft_height,
            4 * m_ft_width * m_ft_height);

    // initialise empty texture of correct size
    m_tex.load_texture(nullptr, m_ft_width, m_ft_height, MIPMAP_NONE);

    // Special case c = 0 for full block.
    {
        m_glyphmap[0] = 0;
        m_glyphs_top = 1;
        m_glyphs_lru = 1; // otherwise LRU algorithm will overwrite 0
        m_glyphs_mru = 0;
        m_glyphs[0].offset  = 0;
        m_glyphs[0].advance = 0;
        m_glyphs[0].ascender = 0;
        m_glyphs[0].renderable = false;
        m_glyphs[0].uchar   = MISSING_CHAR;
        m_glyphs[0].prev    = 0;
        m_glyphs[0].next    = 0;
        for (int x = 0; x < m_max_width; x++)
            for (int y = 0; y < m_max_height; y++)
            {
                unsigned int idx = x + y * m_max_width;
                idx *= 4;
                pixels[idx]     = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            }
        bool success = m_tex.load_texture(pixels, charsz.x, charsz.y,
                                          MIPMAP_NONE, 0, 0);
        ASSERT(success);
    }

    // precache common chars
    for (int i = 0x20; i < 0x7f; i++)
        map_unicode(i);
    return true;
}

void FTFontWrapper::load_glyph(unsigned int c, char32_t uchar)
{
    // get on with rendering the new glyph
    FT_Error error;
    m_glyphs[c].offset  = 0;
    m_glyphs[c].advance = 0;
    m_glyphs[c].ascender = m_ascender;
    m_glyphs[c].renderable = false;

    FT_Int glyph_index = FT_Get_Char_Index(face, uchar);

    if (!glyph_index)
        glyph_index = FT_Get_Char_Index(face, MISSING_CHAR);

    error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER |
        (Options.tile_font_ft_light ? FT_LOAD_TARGET_LIGHT : 0));
    ASSERT(!error);

    FT_Bitmap *bmp = &face->glyph->bitmap;
    ASSERT(bmp);

    int advance = face->glyph->advance.x >> 6;

    // Was int prior to freetype 2.5.4, then became unsigned.
    typedef decltype(bmp->width) ftint;
    ftint bmp_width = bmp->width;
    if (outl)
        bmp_width += 2;

    m_glyphs[c].offset = face->glyph->bitmap_left;
    m_glyphs[c].advance = advance;
    m_glyphs[c].ascender = face->glyph->bitmap_top;
    m_glyphs[c].width = bmp_width;

    // Some glyphs (e.g. ' ') don't get a buffer.
    if (bmp->buffer)
    {
        m_glyphs[c].renderable = true;

        ASSERT(bmp->pixel_mode == FT_PIXEL_MODE_GRAY);
        ASSERT(bmp->num_grays == 256);

        // Horizontal offset stored in m_glyphs and handled when drawing
        const unsigned int offset_x = 0;
        const unsigned int offset_y = 0;
        memset(pixels, 0, sizeof(unsigned char) * 4 * charsz.x * charsz.y);

        // Some fonts have wrong size info
        const ftint charw = bmp->width;
        bmp->width = min(bmp->width, ftint(charsz.x));
        bmp->rows = min(bmp->rows, ftint(charsz.y));

        if (outl)
        {
            for (ftint x = 0; x < bmp->width; x++)
                for (ftint y = 0; y < bmp->rows; y++)
                {
                    unsigned int idx = offset_x+x+1 + (offset_y+y+1) * charsz.x;
                    idx *= 4;

                    unsigned char orig = bmp->buffer[x + charw * y];

                    unsigned char edge = 0;
                    if (x > 0)
                        edge = max(bmp->buffer[(x-1) + charw * y], edge);
                    if (y > 0)
                        edge = max(bmp->buffer[x + charw * (y-1)], edge);
                    if (x < bmp->width - 1)
                        edge = max(bmp->buffer[(x+1) + charw * y], edge);
                    if (y < bmp->rows - 1)
                        edge = max(bmp->buffer[x + charw * (y+1)], edge);

                    pixels[idx] = orig;
                    pixels[idx + 1] = orig;
                    pixels[idx + 2] = orig;
                    pixels[idx + 3] = min((int)orig + edge, 255);
                }
        }
        else
        {
            for (ftint x = 0; x < bmp->width; x++)
                for (ftint y = 0; y < bmp->rows; y++)
                {
                    unsigned int idx = offset_x + x + (offset_y + y) * charsz.x;
                    idx *= 4;
                    if (x < bmp->width && y < bmp->rows)
                    {
                        unsigned char alpha = bmp->buffer[x + charw * y];
                        pixels[idx] = 255;
                        pixels[idx + 1] = 255;
                        pixels[idx + 2] = 255;
                        pixels[idx + 3] = alpha;
                    }
                }
        }
        bool success = m_tex.load_texture(pixels, charsz.x, charsz.y,
                            MIPMAP_NONE,
                            (c % GLYPHS_PER_ROWCOL) * charsz.x,
                            (c / GLYPHS_PER_ROWCOL) * charsz.y);
        ASSERT(success);
    }
}

unsigned int FTFontWrapper::map_unicode(char32_t uchar)
{
    unsigned int c;  // index in m_glyphs
    if (!m_glyphmap.count(uchar))
    {
        // work out which glyph we can overwrite if we've gone over MAX_GLYPHS
        if (m_glyphs_top == MAX_GLYPHS)
        {
            dprintf("replacing %d (%lc) with %d (%lc)\n",
                    m_glyphs[m_glyphs_lru].uchar,
                    m_glyphs[m_glyphs_lru].uchar,
                    uchar,
                    uchar);
            // create a pointer in gmap to the lru entry in gdata
            c = m_glyphs_lru;
            // delete lru glyph from map
            m_glyphmap.erase(m_glyphs[m_glyphs_lru].uchar);
            // move lru on to next
            m_glyphs_lru = m_glyphs[c].next;
            m_glyphs[m_glyphs_lru].prev = 0;
        }
        else // glyph data is not full
        {
            // create a pointer in m_glyphmap to the top of m_glyphs
            c = m_glyphs_top;
            // move top index on
            m_glyphs_top++;
        }

        // set some default prev/next values
        m_glyphs[c].prev = m_glyphs_mru;
        m_glyphs[m_glyphs_mru].next = c;
        m_glyphs[c].next = 0;
        // update links between char and map
        m_glyphs[c].uchar = uchar;
        m_glyphmap[uchar] = c;

        load_glyph(c, uchar);
        n_subst++;

        dprintf("mapped %d (%x; %lc) to %d\n", uchar, uchar, uchar, c);
    }
    else // we found uchar in glyphmap
    {
        c = m_glyphmap[uchar];
        if (m_glyphs_mru != c)
        {
            // point the <char previous to this one> to the <char after this one> and vice-versa
            dprintf("moving %lc: %lc -> %lc; %lc <- %lc",
                    uchar,
                    m_glyphs[m_glyphs[m_glyphmap[uchar]].prev].uchar,
                    m_glyphs[m_glyphs[m_glyphmap[uchar]].next].uchar,
                    m_glyphs[m_glyphs[m_glyphmap[uchar]].next].uchar,
                    m_glyphs[m_glyphs[m_glyphmap[uchar]].prev].uchar);
            m_glyphs[m_glyphs[c].prev].next = m_glyphs[c].next;
            m_glyphs[m_glyphs[c].next].prev = m_glyphs[c].prev;
        }
    }

    // regardless of how we came about 'c'
    if (m_glyphs_mru != c)
    {
        // point the last character we wrote out to the one we're writing
        dprintf("updating %lc, next = %lc",
                m_glyphs[m_glyphs_mru].uchar, uchar);
        m_glyphs[m_glyphs_mru].next = c;
        m_glyphs[c].prev = m_glyphs_mru;
    }

    // update the mru to this one
    m_glyphs_mru = c;
    // if we've just used the lru glyph, move onto the next one
    if (m_glyphs_mru == m_glyphs_lru && m_glyphs[m_glyphs_lru].next != 0)
        m_glyphs_lru = m_glyphs[m_glyphs_lru].next;

    dprintf("rendering %d (%x; <<<<<<%lc>>>>>>); lru is %lc, next lru is %lc\n",
            uchar, uchar, uchar,
            m_glyphs[m_glyphs_lru].uchar,
            m_glyphs[m_glyphs[m_glyphs_lru].next].uchar);

    return m_glyphmap[uchar];
}

void FTFontWrapper::render_textblock(unsigned int x_pos, unsigned int y_pos,
                                     char32_t *chars,
                                     uint8_t *colours,
                                     unsigned int width, unsigned int height,
                                     bool drop_shadow)
{
    if (!chars || !colours || !width || !height || !m_glyphs)
        return;

    coord_def adv(max(-m_min_offset, 0), 0);
    unsigned int i = 0;

    ASSERT(m_buf);
    m_buf->clear();
    n_subst = 0;

    float texcoord_dy = (float)m_max_height / (float)m_tex.height();
    for (unsigned int y = 0; y < height; y++)
    {
        for (unsigned int x = 0; x < width; x++)
        {
            unsigned int c = map_unicode(chars[i]);
            uint8_t col_bg = colours[i] >> 4;
            uint8_t col_fg = colours[i] & 0xF;

            if (col_bg != 0)
            {
                GLWPrim rect(adv.x, adv.y,
                             adv.x + m_max_advance.x, adv.y + m_max_advance.y);
                // Leave tex coords at their default 0.0f
                VColour col(term_colours[col_bg].r,
                            term_colours[col_bg].g,
                            term_colours[col_bg].b);
                rect.set_col(col);
                m_buf->add(rect);
            }

            adv.x += m_glyphs[c].offset;

            if (m_glyphs[c].renderable)
            {
                int this_width = m_glyphs[c].width;

                float tex_x = (float)(c % GLYPHS_PER_ROWCOL) / (float)GLYPHS_PER_ROWCOL;
                float tex_y = (float)(c / GLYPHS_PER_ROWCOL) / (float)GLYPHS_PER_ROWCOL;
                float tex_x2 = tex_x + (float)this_width / (float)m_tex.width();
                float tex_y2 = tex_y + texcoord_dy;

                GLWPrim rect(adv.x, adv.y - m_glyphs[c].ascender + m_ascender,
                             adv.x + this_width, adv.y + m_max_height - m_glyphs[c].ascender + m_ascender);

                VColour col(term_colours[col_fg].r,
                            term_colours[col_fg].g,
                            term_colours[col_fg].b);
                rect.set_col(col);
                rect.set_tex(tex_x, tex_y, tex_x2, tex_y2);

                m_buf->add(rect);
            }

            i++;
            adv.x += m_glyphs[c].advance - m_glyphs[c].offset;

            // See if we need to flush prematurely.
            if (n_subst == MAX_GLYPHS - 1)
            {
                draw_m_buf(x_pos, y_pos, drop_shadow);
                m_buf->clear();
                n_subst = 0;
            }
        }

        adv.x = 0;
        adv.y += m_max_advance.y;
    }

    draw_m_buf(x_pos, y_pos, drop_shadow);
}

void FTFontWrapper::draw_m_buf(unsigned int x_pos, unsigned int y_pos,
                               bool drop_shadow)
{
    if (!m_buf->size())
        return;

    GLState state;
    state.array_vertex = true;
    state.array_texcoord = true;
    state.array_colour = true;
    state.blend = true;
    state.texture = true;

    m_tex.bind();

    GLW_3VF trans(x_pos, y_pos, 0.0f);
    GLW_3VF scale((float)scale_den / (float)scale_num,
                  (float)scale_den / (float)scale_num, 1);

    if (drop_shadow)
    {
        GLState state_shadow;
        state_shadow.array_colour = false;
        state_shadow.colour = VColour::black;

        GLW_3VF trans_shadow(trans.x + 1, trans.y + 1, 0.0f);
        glmanager->set_transform(trans_shadow, scale);

        m_buf->draw(state_shadow);
    }

    glmanager->set_transform(trans, scale);
    m_buf->draw(state);

    glmanager->reset_transform();
}

static void _draw_box(int x_pos, int y_pos, float width, float height,
                      float box_width, unsigned char box_colour,
                      unsigned char box_alpha)
{
    unique_ptr<GLShapeBuffer> buf(GLShapeBuffer::create(false, true));
    GLWPrim rect(x_pos - box_width, y_pos - box_width,
                 x_pos + width + box_width, y_pos + height + box_width);

    VColour colour(term_colours[box_colour].r,
                   term_colours[box_colour].g,
                   term_colours[box_colour].b,
                   box_alpha);
    rect.set_col(colour);

    buf->add(rect);

    // Load identity matrix
    glmanager->reset_transform();

    GLState state;
    state.array_vertex = true;
    state.array_colour = true;
    state.blend = true;
    buf->draw(state);
}

unsigned int FTFontWrapper::string_height(const formatted_string &str) const
{
    string temp = str.tostring();
    return string_height(temp.c_str());
}

unsigned int FTFontWrapper::string_height(const char *text) const
{
    int height = 1;
    for (const char *itr = text; (*itr); itr++)
        if (*itr == '\n')
            height++;

    return char_height() * height;
}

unsigned int FTFontWrapper::string_width(const formatted_string &str)
{
    string temp = str.tostring();
    return string_width(temp.c_str());
}

unsigned int FTFontWrapper::string_width(const char *text)
{
    unsigned int base_width = max(-m_min_offset, 0);
    unsigned int max_width = 0;

    unsigned int width = base_width;
    unsigned int adjust = 0;
    for (const unsigned char *itr = (unsigned const char *)text; *itr; itr++)
    {
        if (*itr == '\n')
        {
            max_width = max(width + adjust, max_width);
            width = base_width;
            adjust = 0;
        }
        else
        {
            unsigned int c = map_unicode(*itr);
            width += m_glyphs[c].advance;
            adjust = max(0, m_glyphs[c].width - m_glyphs[c].advance);
        }
    }

    max_width = max(width + adjust, max_width);
    return max_width * scale_den / scale_num;
}

int FTFontWrapper::find_index_before_width(const char *text, int max_width)
{
    int width = max(-m_min_offset, 0);

    max_width *= scale_num / scale_den;

    for (int i = 0; text[i]; i++)
    {
        unsigned int c = map_unicode(text[i]);
        width += m_glyphs[c].advance;
        int adjust = max(0, m_glyphs[c].width - m_glyphs[c].advance);
        if (width + adjust > max_width)
            return i;
    }

    return -1;
}

formatted_string FTFontWrapper::split(const formatted_string &str,
                                      unsigned int max_width,
                                      unsigned int max_height)
{
    int max_lines = max_height / char_height();

    if (max_lines < 1)
        return formatted_string();

    formatted_string ret;
    ret += str;

    string base = str.tostring();
    int num_lines = 0;

    char *line = &base[0];
    while (true)
    {
        int line_end = find_index_before_width(line, max_width);
        if (line_end == -1)
            break;

        int space_idx = 0;
        for (char *search = &line[line_end]; search > line; search--)
        {
            if (*search == ' ')
            {
                space_idx = search - line;
                break;
            }
        }

        if (++num_lines >= max_lines || !space_idx)
        {
            int ellipses;
            if (space_idx && space_idx - line_end > 2)
                ellipses = space_idx;
            else
                ellipses = line_end - 2;

            size_t idx = &line[ellipses] - &base[0];
            ret = ret.chop(idx);
            ret += formatted_string("..");
            return ret;
        }
        else
        {
            line[space_idx] = '\n';
            ret[&line[space_idx] - &base[0]] = '\n';
        }

        line = &line[space_idx+1];
    }

    return ret;
}

void FTFontWrapper::render_string(unsigned int px, unsigned int py,
                                  const char *text,
                                  const coord_def &min_pos,
                                  const coord_def &max_pos,
                                  unsigned char font_colour, bool drop_shadow,
                                  unsigned char box_alpha,
                                  unsigned char box_colour,
                                  unsigned int outline,
                                  bool tooltip)
{
    ASSERT(text);

    // Determine extent of this text
    unsigned int max_rows = 1;
    unsigned int cols = 0;
    unsigned int max_cols = 0;
    char32_t c;
    for (const char *tp = text; int s = utf8towc(&c, tp); tp += s)
    {
        int w = wcwidth(c);
        if (w != -1)
            cols += w;
        max_cols = max(cols, max_cols);

        // NOTE: only newlines should be used for tool tips. Don't use EOL.
        ASSERT(c != '\r');

        if (c == '\n')
        {
            cols = 0;
            max_rows++;
        }
    }

    // Create the text block
    char32_t *chars = (char32_t*)malloc(max_rows * max_cols * sizeof(char32_t));
    uint8_t *colours = (uint8_t*)malloc(max_rows * max_cols);
    for (unsigned int i = 0; i < max_rows * max_cols; i++)
        chars[i] = ' ';
    memset(colours, font_colour, max_rows * max_cols);

    // Fill the text block
    cols = 0;
    unsigned int rows = 0;
    for (const char *tp = text; int s = utf8towc(&c, tp); tp += s)
    {
        int w = wcwidth(c);
        if (w > 0) // FIXME: combining characters are silently ignored
        {
            chars[cols + rows * max_cols] = c;
            cols++;
            if (w == 2)
                chars[cols + rows * max_cols] = ' ', cols++;
        }

        if (c == '\n')
        {
            cols = 0;
            rows++;
        }
    }

    // Find a suitable location on screen
    const int buffer = 5;  // additional buffer size from edges

    int wx = string_width(text);
    int wy = max_rows * char_height();

    int sx, sy; // box starting location, uses extra buffer
    int tx, ty; // text starting location

    if (tooltip)
    {
        sy = py + outline;
        ty = sy + buffer;
        tx = px - 20;
        sx = tx - buffer;
    }
    else
    {
        ty = py - wy - outline;
        sy = ty - buffer;
        tx = px - wx / 2;
        sx = tx - buffer;
    }
    // box ending position
    int ex = tx + wx + buffer;
    int ey = ty + wy + buffer;

    if (ex > max_pos.x)
        tx += max_pos.x - ex;
    else if (sx < min_pos.x)
        tx -= sx - min_pos.x;

    if (ey > max_pos.y)
        ty += max_pos.y - ey;
    else if (sy < min_pos.y)
        ty -= sy - min_pos.y;

    if (box_alpha != 0)
        _draw_box(tx, ty, wx, wy, outline, box_colour, box_alpha);

    render_textblock(tx, ty, chars, colours, max_cols, max_rows, drop_shadow);

    free(chars);
    free(colours);
}

void FTFontWrapper::store(FontBuffer &buf, float &x, float &y,
                          const string &str, const VColour &col)
{
    store(buf, x, y, str, col, x);
}

void FTFontWrapper::store(FontBuffer &buf, float &x, float &y,
                          const string &str, const VColour &col, float orig_x)
{
    const char *sp = str.c_str();
    char32_t c;
    while (int s = utf8towc(&c, sp))
    {
        sp += s;
        if (c == '\n')
        {
            x = orig_x;
            y += m_max_advance.y * scale_den / scale_num;
        }
        else
            store(buf, x, y, c, col);
    }
}

void FTFontWrapper::store(FontBuffer &buf, float &x, float &y,
                          const formatted_string &fs)
{
    store(buf, x, y, fs, x);
}

void FTFontWrapper::store(FontBuffer &buf, float &x, float &y,
                          const formatted_string &fs, float orig_x)
{
    int colour = LIGHTGREY;
    for (const formatted_string::fs_op &op : fs.ops)
    {
        switch (op.type)
        {
            case FSOP_COLOUR:
                // Only foreground colors for now...
                colour = op.x & 0xF;
                break;
            case FSOP_TEXT:
                store(buf, x, y, op.text, term_colours[colour], orig_x);
                break;
            default:
                break;
        }
    }
}

void FTFontWrapper::store(FontBuffer &buf, float &x, float &y,
                          char32_t ch, const VColour &col)
{
    unsigned int c = map_unicode(ch);
    if (!m_glyphs[c].renderable)
    {
        x += m_glyphs[c].advance * scale_den / scale_num;
        return;
    }

    int this_width = m_glyphs[c].width;

    float pos_sx = x + m_glyphs[c].offset * (float)scale_den / (float)scale_num;
    float pos_sy = y - (m_glyphs[c].ascender - m_ascender) * (float)scale_den
                                                           / (float)scale_num;
    float pos_ex = pos_sx + this_width * (float)scale_den / (float)scale_num;
    float pos_ey = y + (m_max_height - m_glyphs[c].ascender + m_ascender)
                   * (float)scale_den / (float)scale_num;

    float tex_sx = (float)(c % GLYPHS_PER_ROWCOL) / (float)GLYPHS_PER_ROWCOL;
    float tex_sy = (float)(c / GLYPHS_PER_ROWCOL) / (float)GLYPHS_PER_ROWCOL;
    float tex_ex = tex_sx + (float)this_width / (float)(GLYPHS_PER_ROWCOL*charsz.x);
    float tex_ey = tex_sy + (float)m_max_height / (float)(GLYPHS_PER_ROWCOL*charsz.y);

    GLWPrim rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_tex(tex_sx, tex_sy, tex_ex, tex_ey);
    rect.set_col(col);
    buf.add_primitive(rect);

    x += m_glyphs[c].advance * (float)scale_den / (float)scale_num;
}

unsigned int FTFontWrapper::char_width() const
{
    return m_max_advance.x * scale_den / scale_num;
}

unsigned int FTFontWrapper::char_height() const
{
    return m_max_advance.y * scale_den / scale_num;
}

const GenericTexture *FTFontWrapper::font_tex() const
{
    return &m_tex;
}

#endif // USE_FT
#endif // USE_TILE_LOCAL

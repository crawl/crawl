#include "AppHdr.h"

#ifdef USE_TILE
#ifdef USE_FT

#include <ft2build.h>
#include FT_FREETYPE_H

#include "defines.h"
#include "files.h"
#include "format.h"
#include "fontwrapper-ft.h"
#include "glwrapper.h"
#include "tilebuf.h"
#include "tilefont.h"

FontWrapper* FontWrapper::create()
{
    return (new FTFontWrapper());
}

FTFontWrapper::FTFontWrapper() :
    m_glyphs(NULL),
    m_max_advance(0, 0),
    m_min_offset(0)
{
    m_buf = GLShapeBuffer::create(true, true);
}

FTFontWrapper::~FTFontWrapper()
{
    delete[] m_glyphs;
    delete m_buf;
}

bool FTFontWrapper::load_font(const char *font_name, unsigned int font_size,
                              bool outl)
{
    FT_Library library;
    FT_Face face;
    FT_Error error;

    error = FT_Init_FreeType(&library);
    if (error)
    {
        fprintf(stderr, "Failed to initialise freetype library.\n");
        return false;
    }

    // TODO enne - need to find a cross-platform way to also
    // attempt to locate system fonts by name...
    std::string font_path = datafile_path(font_name, false, true);
    if (font_path.c_str()[0] == 0)
    {
        fprintf(stderr, "Could not find font '%s'\n", font_name);
        return false;
    }

    error = FT_New_Face(library, font_path.c_str(), 0, &face);
    if (error == FT_Err_Unknown_File_Format)
    {
        fprintf(stderr, "Unknown font format for file '%s'\n",
                         font_path.c_str());
        return false;
    }
    else if (error)
    {
        fprintf(stderr, "Invalid font from file '%s'\n", font_path.c_str());
    }

    error = FT_Set_Pixel_Sizes(face, font_size, font_size);
    ASSERT(!error);

    // Get maximum advance
    m_max_advance = coord_def(0,0);
    int ascender  = face->ascender >> 6;
    int min_y     = 100000;
    int max_y     = 0;
    int max_width = 0;
    m_min_offset  = 0;
    m_glyphs      = new GlyphInfo[256];
    for (unsigned int c = 0; c < 256; c++)
    {
        m_glyphs[c].offset  = 0;
        m_glyphs[c].advance = 0;
        m_glyphs[c].renderable = false;

        FT_Int glyph_index = FT_Get_Char_Index(face, c);
        if (!glyph_index)
            continue;

        error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        ASSERT(!error);

        FT_Bitmap *bmp = &face->glyph->bitmap;

        int advance = face->glyph->advance.x >> 6;

        m_max_advance.x = std::max(m_max_advance.x, advance);

        int bmp_width  = bmp->width;
        int bmp_top    = ascender - face->glyph->bitmap_top;
        int bmp_bottom = ascender + bmp->rows - face->glyph->bitmap_top;
        if (outl)
        {
            bmp_width  += 2;
            bmp_top    -= 1;
            bmp_bottom += 1;
        }

        max_width = std::max(max_width, bmp_width);
        min_y = std::min(min_y, bmp_top);
        max_y = std::max(max_y, bmp_bottom);

        m_glyphs[c].offset  = face->glyph->bitmap_left;
        m_glyphs[c].advance = advance;
        m_glyphs[c].width   = bmp_width;

        m_min_offset = std::min((char)m_min_offset, m_glyphs[c].offset);
    }

    // The ascender and text height given by FreeType2 is ridiculously large
    // (e.g. 37 pixels high for 14 pixel font).  Use min and max bounding
    // heights on the characters we care about to get better values for the
    // text height and the ascender.
    m_max_advance.y = max_y - min_y;
    ascender       -= min_y;

    int max_height  = m_max_advance.y;

    // Grow character size to power of 2
    coord_def charsz(1,1);
    while (charsz.x < max_width)
        charsz.x *= 2;
    while (charsz.y < max_height)
        charsz.y *= 2;

    // Fill out texture to be (16*charsz.x) X (16*charsz.y) X (32-bit)
    // Having to blow out 8-bit alpha values into full 32-bit textures is
    // kind of frustrating, but not all OpenGL implementations support the
    // "esoteric" ALPHA8 format and it's not like this texture is very large.
    unsigned int   width  = 16 * charsz.x;
    unsigned int   height = 16 * charsz.y;
    unsigned char *pixels = new unsigned char[4 * width * height];
    memset(pixels, 0, sizeof(unsigned char) * 4 * width * height);

    // Special case c = 0 for full block.
    {
        m_glyphs[0].renderable = false;
        for (int x = 0; x < max_width; x++)
            for (int y = 0; y < max_height; y++)
            {
                unsigned int idx = x + y * width;
                idx *= 4;
                pixels[idx] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            }
    }

    for (unsigned int c = 1; c < 256; c++)
    {
        FT_Int glyph_index = FT_Get_Char_Index(face, c);
        if (!glyph_index)
        {
            // If no mapping for this character, leave blank.
            continue;
        }

        error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        ASSERT(!error);

        FT_Bitmap *bmp = &face->glyph->bitmap;
        ASSERT(bmp);
        // Some glyphs (e.g. ' ') don't get a buffer.
        if (!bmp->buffer)
            continue;

        m_glyphs[c].renderable = true;

        int vert_offset = ascender - face->glyph->bitmap_top;

        ASSERT(bmp->pixel_mode == FT_PIXEL_MODE_GRAY);
        ASSERT(bmp->num_grays == 256);

        // Horizontal offset stored in m_glyphs and handled when drawing
        unsigned int offset_x = (c % 16) * charsz.x;
        unsigned int offset_y = (c / 16) * charsz.y + vert_offset;

        if (outl)
        {
            const int charw = bmp->width;
            for (int x = -1; x <= bmp->width; x++)
                for (int y = -1; y <= bmp->rows; y++)
                {
                    bool x_valid = x >= 0 && x < bmp->width;
                    bool y_valid = y >= 0 && y < bmp->rows;
                    bool valid   = x_valid && y_valid;
                    unsigned char orig = valid ? bmp->buffer[x + charw * y] : 0;

                    unsigned char edge = 0;
                    if (y_valid && x > 0)
                        edge = std::max(bmp->buffer[(x-1) + charw * y], edge);
                    if (x_valid && y > 0)
                        edge = std::max(bmp->buffer[x + charw * (y-1)], edge);
                    if (y_valid && x < bmp->width - 1)
                        edge = std::max(bmp->buffer[(x+1) + charw * y], edge);
                    if (x_valid && y < bmp->rows - 1)
                        edge = std::max(bmp->buffer[x + charw * (y+1)], edge);

                    unsigned int idx = offset_x+x+1 + (offset_y+y+1) * width;
                    idx *= 4;

                    pixels[idx] = orig;
                    pixels[idx + 1] = orig;
                    pixels[idx + 2] = orig;
                    pixels[idx + 3] = std::min(orig + edge, 255);
                }
        }
        else
        {
            for (int x = 0; x < bmp->width; x++)
                for (int y = 0; y < bmp->rows; y++)
                {
                    unsigned int idx = offset_x + x + (offset_y + y) * width;
                    idx *= 4;
                    unsigned char alpha = bmp->buffer[x + bmp->width * y];
                    pixels[idx] = 255;
                    pixels[idx + 1] = 255;
                    pixels[idx + 2] = 255;
                    pixels[idx + 3] = alpha;
                }
        }
    }

    bool success = m_tex.load_texture(pixels, width, height,
                                      MIPMAP_NONE);

    delete[] pixels;

    return success;
}

void FTFontWrapper::render_textblock(unsigned int x_pos, unsigned int y_pos,
                                     unsigned char *chars,
                                     unsigned char *colours,
                                     unsigned int width, unsigned int height,
                                     bool drop_shadow)
{
    if (!chars || !colours || !width || !height || !m_glyphs)
        return;

    coord_def adv(std::max(-m_min_offset, 0), 0);
    unsigned int i = 0;

    ASSERT(m_buf);
    m_buf->clear();

    float texcoord_dy = (float)m_max_advance.y / (float)m_tex.height();

    for (unsigned int y = 0; y < height; y++)
    {
        for (unsigned int x = 0; x < width; x++)
        {
            unsigned char c = chars[i];
            unsigned char col_bg = colours[i] >> 4;
            unsigned char col_fg = colours[i] & 0xF;

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

                float tex_x = (float)(c % 16) / 16.0f;
                float tex_y = (float)(c / 16) / 16.0f;
                float tex_x2 = tex_x + (float)this_width / (float)m_tex.width();
                float tex_y2 = tex_y + texcoord_dy;

                GLWPrim rect(adv.x, adv.y, adv.x + this_width,
                             adv.y + m_max_advance.y);
                VColour col(term_colours[col_fg].r,
                            term_colours[col_fg].g,
                            term_colours[col_fg].b);
                rect.set_col(col);
                rect.set_tex(tex_x, tex_y, tex_x2, tex_y2);

                m_buf->add(rect);
            }

            i++;
            adv.x += m_glyphs[c].advance - m_glyphs[c].offset;
        }

        adv.x = 0;
        adv.y += m_max_advance.y;
    }

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
    GLW_3VF scale(1, 1, 1);

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
}

static void _draw_box(int x_pos, int y_pos, float width, float height,
                      float box_width, unsigned char box_colour,
                      unsigned char box_alpha)
{
    std::auto_ptr<GLShapeBuffer> buf(GLShapeBuffer::create(false, true));
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
    std::string temp = str.tostring();
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

unsigned int FTFontWrapper::string_width(const formatted_string &str) const
{
    std::string temp = str.tostring();
    return string_width(temp.c_str());
}

unsigned int FTFontWrapper::string_width(const char *text) const
{
    unsigned int base_width = std::max(-m_min_offset, 0);
    unsigned int max_width = 0;

    unsigned int width = base_width;
    unsigned int adjust = 0;
    for (const unsigned char *itr = (unsigned const char *)text; *itr; itr++)
    {
        if (*itr == '\n')
        {
            max_width = std::max(width + adjust, max_width);
            width = base_width;
            adjust = 0;
        }
        else
        {
            width += m_glyphs[*itr].advance;
            adjust = std::max(0, m_glyphs[*itr].width - m_glyphs[*itr].advance);
        }
    }

    max_width = std::max(width + adjust, max_width);
    return max_width;
}

int FTFontWrapper::find_index_before_width(const char *text, int max_width)
{
    int width = std::max(-m_min_offset, 0);

    for (int i = 0; text[i]; i++)
    {
        unsigned char c = text[i];
        width += m_glyphs[c].advance;
        int adjust = std::max(0, m_glyphs[c].width - m_glyphs[c].advance);
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

    std::string base = str.tostring();
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
            if (space_idx && (space_idx - line_end > 2))
                ellipses = space_idx;
            else
                ellipses = line_end - 2;

            size_t idx = &line[ellipses] - &base[0];
            ret[idx] = '.';
            ret[idx+1] = '.';

            return ret.substr(0, idx + 2);
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
    for (const char *itr = text; *itr; itr++)
    {
        cols++;
        max_cols = std::max(cols, max_cols);

        // NOTE: only newlines should be used for tool tips.  Don't use EOL.
        ASSERT(*itr != '\r');

        if (*itr == '\n')
        {
            cols = 0;
            max_rows++;
        }
    }

    // Create the text block
    unsigned char *chars = (unsigned char *)alloca(max_rows * max_cols);
    unsigned char *colours = (unsigned char *)alloca(max_rows * max_cols);
    memset(chars, ' ', max_rows * max_cols);
    memset(colours, font_colour, max_rows * max_cols);

    // Fill the text block
    cols = 0;
    unsigned int rows = 0;
    for (const char *itr = text; *itr; itr++)
    {
        chars[cols + rows * max_cols] = *itr;
        cols++;

        if (*itr == '\n')
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

    tx = px - wx / 2;
    sx = tx - buffer;
    if (tooltip)
    {
        sy = py + outline;
        ty = sy + buffer;
    }
    else
    {
        ty = py - wy - outline;
        sy = ty - buffer;
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
}

void FTFontWrapper::store(FontBuffer &buf, float &x, float &y,
                          const std::string &str, const VColour &col)
{
    store(buf, x, y, str, col, x);
}

void FTFontWrapper::store(FontBuffer &buf, float &x, float &y,
                          const std::string &str, const VColour &col,
                          float orig_x)
{
    for (unsigned int i = 0; i < str.size(); i++)
    {
        char c = str[i];
        if (c == '\n')
        {
            x = orig_x;
            y += m_max_advance.y;
        }
        else
        {
            store(buf, x, y, c, col);
        }
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
    for (unsigned int i = 0; i < fs.ops.size(); i++)
    {
        switch (fs.ops[i].type)
        {
            case FSOP_COLOUR:
                // Only foreground colors for now...
                colour = fs.ops[i].x & 0xF;
                break;
            case FSOP_TEXT:
                store(buf, x, y, fs.ops[i].text, term_colours[colour], orig_x);
                break;
            default:
                break;
        }
    }
}

void FTFontWrapper::store(FontBuffer &buf, float &x, float &y,
                          unsigned char c, const VColour &col)
{
    if (!m_glyphs[c].renderable)
    {
        x += m_glyphs[c].advance;
        return;
    }

    int this_width = m_glyphs[c].width;

    float pos_sx = x + m_glyphs[c].offset;
    float pos_sy = y;
    float pos_ex = pos_sx + this_width;
    float pos_ey = y + m_max_advance.y;

    float tex_sx = (float)(c % 16) / 16.0f;
    float tex_sy = (float)(c / 16) / 16.0f;
    float tex_ex = tex_sx + (float)this_width / (float)m_tex.width();
    float tex_ey = tex_sy + (float)m_max_advance.y / (float)m_tex.height();

    GLWPrim rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_tex(tex_sx, tex_sy, tex_ex, tex_ey);
    rect.set_col(col);
    buf.add_primitive(rect);

    x += m_glyphs[c].advance;
}

unsigned int FTFontWrapper::char_width() const
{
    return (m_max_advance.x);
}

unsigned int FTFontWrapper::char_height() const
{
    return (m_max_advance.y);
}

const GenericTexture *FTFontWrapper::font_tex() const
{
    return (&m_tex);
}

#endif // USE_FT
#endif // USE_TILE

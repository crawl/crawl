#include "tile_list_processor.h"

#include <cassert>
#include <cctype>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <algorithm>

using namespace std;

tile_list_processor::tile_list_processor() :
    m_last_enum(0),
    m_rim(false),
    m_corpsify(false),
    m_composing(false),
    m_shrink(true),
    m_prefix("TILE"),
    m_start_value("0"),
    m_start_value_module(""),
    m_texture(0),
    m_variation_idx(-1),
    m_variation_col(-1),
    m_weight(1),
    m_alpha(0.0),
    m_domino(0)
{
}

tile_list_processor::~tile_list_processor()
{
    for (unsigned int i = 0; i < m_back.size(); i++)
        delete m_back[i];

    m_back.resize(0);
    if (m_texture)
        delete m_texture;
}

bool tile_list_processor::load_image_from_tile(tile &img, string filename)
{
    if (filename.substr(0, 5) != "enum:")
        return false;
    const string enumname = filename.substr(5);

    int idx = m_page.find(enumname);
    if (idx == -1)
        return false;

    img.copy(*m_page.m_tiles[idx]);
    for (int i = 0; i < MAX_COLOUR; ++i)
        img.add_variation(i, -1);
    return true;
}

bool tile_list_processor::load_image(tile &img, const char *filename,
                                     bool background)
{
    assert(filename);

    if (load_image_from_tile(img, filename))
        return true;

    char temp[1024];

    const unsigned int num_ext = 3;
    const char *ext[3] =
    {
        ".png",
        ".bmp",
        ""
    };

    if (m_sdir != "" || background && m_back_sdir != "")
    {
        vector<const char *> dirs;
        if (m_sdir != "")
            dirs.push_back(m_sdir.c_str());
        if (background && m_back_sdir != "")
            dirs.push_back(m_back_sdir.c_str());
        for (unsigned int e = 0; e < num_ext; e++)
        {
            for (unsigned int d = 0; d < dirs.size(); d++)
            {
                sprintf(temp, "%s/%s%s", dirs[d], filename, ext[e]);
                if (img.load(temp))
                {
                    m_depends.push_back(temp);
                    return true;
                }
            }
        }
    }

    for (unsigned int e = 0; e < num_ext; e++)
    {
        sprintf(temp, "%s%s", filename, ext[e]);
        if (img.load(temp))
        {
            m_depends.push_back(temp);
            return true;
        }
    }

    return false;
}

bool tile_list_processor::process_list(const char *list_file)
{
    m_depends.push_back(list_file);

    ifstream input(list_file);
    if (!input.is_open())
    {
        fprintf(stderr, "Error: couldn't open '%s' for read.\n", list_file);
        return false;
    }

    const size_t bufsize = 1024;
    char read_line[bufsize];
    int line     = 1;
    bool success = true;
    while (!input.getline(read_line, bufsize).eof())
        success &= process_line(read_line, list_file, line++);

    return success;
}

static void eat_whitespace(char *&text)
{
    if (!text)
        return;

    while (*text)
    {
        if (*text != ' ' && *text != '\n' && *text != '\r')
            break;
        text++;
    }

    if (!*text)
        return;

    char *idx = &text[strlen(text) - 1];
    while (*idx)
    {
        if (*idx != ' ' && *idx != '\n' && *idx != '\r')
            break;
        *idx = 0;
        idx--;
    }
}

static void eat_comments(char *&text)
{
    if (!text)
        return;

    char *idx = text;
    while (*idx)
    {
        if (idx[0] == '/' && idx[1] == '*')
        {
            char *end = idx + 2;
            bool found = false;
            while (*end)
            {
                if (end[0] == '*' && end[1] == '/')
                {
                    found = true;

                    end += 2;
                    char *begin = idx;
                    while (*end)
                    {
                        *begin = *end;
                        begin++;
                        end++;
                    }
                    *begin = 0;
                }

                end++;
            }

            if (!found)
            {
                *idx = 0;
                break;
            }
        }

        idx++;
    }
}

static const string colour_list[16] =
{
    "black", "blue", "green", "cyan", "red", "magenta", "brown",
    "lightgrey", "darkgrey", "lightblue", "lightgreen", "lightcyan",
    "lightred", "lightmagenta", "yellow", "white"
};

static int str_to_colour(string colour)
{
    if (colour.empty())
        return 0;

    for (unsigned int c = 0; c < colour.size(); c++)
        colour[c] = toalower(colour[c]);

    for (int i = 0; i < 16; ++i)
    {
        if (colour == colour_list[i])
            return i;
    }

    // Check for alternate spellings.
    if (colour == "lightgray")
        return 7;
    else if (colour == "darkgray")
        return 8;

    return 0;
}

void tile_list_processor::recolour(tile &img)
{
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            tile_colour &col = img.get_pixel(x, y);
            tile_colour orig = col;
            for (palette_list::iterator iter = m_palette.begin();
                 iter != m_palette.end(); ++iter)
            {
                if (orig == iter->first)
                    col = iter->second;
            }

            for (hue_list::iterator iter = m_hues.begin();
                 iter != m_hues.end(); ++iter)
            {
                if (orig.get_hue() == iter->first)
                    col.set_hue(iter->second);
            }

            for (desat_list::iterator iter = m_desat.begin();
                iter != m_desat.end(); ++iter)
            {
                if (orig.get_hue() == *iter)
                    col.desaturate();
            }

            for (lum_list::iterator iter = m_lum.begin();
                iter != m_lum.end(); ++iter)
            {
                if (orig.get_hue() == iter->first)
                    col.change_lum(iter->second);
            }

            // Ignore 0 alpha since it'd just set the whole image transparent
            if (m_alpha > 0.0)
                col.a = min(255.0,(double)col.a * m_alpha);
        }
}

bool tile_list_processor::process_line(char *read_line, const char *list_file,
                                       int line)
{
    eat_comments(read_line);

    const char *delim = " ";
    char *arg;

    arg = strtok(read_line, delim);
    if (!arg)
        return true;

    eat_whitespace(arg);

    if (!*arg)
        return true;

    if (arg[0] == '#')
        return true;

    vector<char *> m_args;
    m_args.push_back(arg);

    while (char *extra = strtok(nullptr, delim))
    {
        eat_whitespace(extra);
        if (!*extra)
            continue;

        m_args.push_back(extra);
    }

    if (arg[0] == '%')
    {
        arg++;

        #define CHECK_NO_ARG(x) \
            if (m_args.size() > x) \
            { \
                fprintf(stderr, "Error (%s:%d): " \
                    "invalid args following '%s'.\n", \
                    list_file, line, arg); \
                return false; \
            }

        #define CHECK_ARG(x) \
            if (m_args.size() <= x) \
            { \
                fprintf(stderr, "Error (%s:%d): " \
                    "missing arg following '%s'.\n", \
                    list_file, line, arg); \
                return false; \
            }

        if (strcmp(arg, "back") == 0)
        {
            CHECK_ARG(1);

            for (unsigned int i = 0; i < m_back.size(); i++)
                delete m_back[i];

            m_back.resize(0);

            if (strcmp(m_args[1], "none") == 0)
            {
                CHECK_NO_ARG(2);
                return true;
            }

            for (unsigned int i = 1; i < m_args.size(); i++)
            {
                tile *img = new tile();
                if (!load_image(*img, m_args[i], true))
                {
                    fprintf(stderr, "Error(%s:%d): couldn't load image "
                                    "'%s'.\n", list_file, line, m_args[i]);
                    return false;
                }
                m_back.push_back(img);
            }
        }
        else if (strcmp(arg, "compose") == 0)
        {
            CHECK_ARG(1);
            if (!m_composing)
            {
                fprintf(stderr, "Error (%s:%d): not composing yet.\n",
                        list_file, line);
                return false;
            }

            if (m_compose.valid())
            {
                tile img;
                if (!load_image(img, m_args[1]))
                {
                    fprintf(stderr, "Error(%s:%d): couldn't load image "
                                    "'%s'.\n", list_file, line, m_args[1]);
                    return false;
                }

                if (m_rim)
                    img.add_rim(tile_colour::black);

                recolour(img);

                if (!m_compose.compose(img))
                {
                    fprintf(stderr, "Error (%s:%d): failed composing '%s'"
                                    " onto compose image.\n",
                            list_file, line, m_args[1]);
                    return false;
                }
            }
            else
            {
                if (!load_image(m_compose, m_args[1]))
                {
                    fprintf(stderr, "Error(%s:%d): couldn't load image "
                                    "'%s'.\n", list_file, line, m_args[1]);
                    return false;
                }

                recolour(m_compose);
            }
        }
        else if (strcmp(arg, "corpse") == 0)
        {
            CHECK_ARG(1);
            m_corpsify = (bool)atoi(m_args[1]);
        }
        else if (strcmp(arg, "end_ctg") == 0)
        {
            CHECK_NO_ARG(1);

            if (m_parts_ctg.empty())
            {
                fprintf(stderr, "Error (%s:%d): no category to end.\n",
                        list_file, line);
                return false;
            }

            m_parts_ctg.clear();
        }
        else if (strcmp(arg, "finish") == 0)
        {
            if (!m_composing)
            {
                fprintf(stderr, "Error (%s:%d): not composing yet.\n",
                        list_file, line);
                return false;
            }

            if (m_corpsify)
                m_compose.corpsify();
            else if (m_rim)
                m_compose.add_rim(tile_colour::black);

            if (m_texture)
                m_compose.texture(*m_texture);

            if (!m_back.empty())
            {
                const unsigned int pseudo_rand = m_page.m_tiles.size() * 54321;
                tile *back = m_back[pseudo_rand % m_back.size()];
                tile img(*back);
                if (!img.compose(m_compose))
                {
                    fprintf(stderr, "Error (%s:%d): failed composing '%s'"
                                    " onto back image '%s'.\n",
                            list_file, line, arg, back->filename().c_str());
                    return false;
                }
                add_image(img, m_args.size() > 1 ? m_args[1] : nullptr);
            }
            else
                add_image(m_compose, m_args.size() > 1 ? m_args[1] : nullptr);

            m_compose.unload();
            m_composing = false;
        }
        else if (strcmp(arg, "texture") == 0)
        {
            CHECK_ARG(1);

            if (m_texture)
                delete m_texture;

            if (strcmp(m_args[1], "none") == 0)
            {
                CHECK_NO_ARG(2);
                m_texture = 0;
                return true;
            }

            CHECK_NO_ARG(2);
            tile *img = new tile();
            if (!load_image(*img, m_args[1], true))
            {
                fprintf(stderr, "Error(%s:%d): couldn't load image "
                                "'%s'.\n", list_file, line, m_args[1]);
                return false;
            }
            m_texture = img;
        }
        else if (strcmp(arg, "include") == 0)
        {
            CHECK_ARG(1);
            if (!process_list(m_args[1]))
            {
                fprintf(stderr, "Error (%s:%d): include failed.\n",
                        list_file, line);
                return false;
            }
        }
        else if (strcmp(arg, "name") == 0)
        {
            CHECK_ARG(1);

            if (m_name != "")
            {
                fprintf(stderr,
                        "Error (%s:%d): name already specified as '%s'\n",
                        list_file, line, m_name.c_str());
                return false;
            }

            m_name = m_args[1];
        }
        else if (strcmp(arg, "abstract") == 0)
        {
            CHECK_ARG(2);

            // filename / prefix pairs.
            m_abstract.push_back(string_pair(m_args[1], m_args[2]));
            char temp[1024];
            sprintf(temp, "tiledef-%s.h", m_args[1]);
            m_include.push_back(temp);
        }
        else if (strcmp(arg, "parts_ctg") == 0)
        {
            CHECK_ARG(1);

            for (unsigned int i = 0; i < m_categories.size(); i++)
            {
                if (m_args[1] == m_categories[i])
                {
                    fprintf(stderr,
                            "Error (%s:%d): category '%s' already used.\n",
                            list_file, line, m_args[1]);
                    return false;
                }
            }

            m_parts_ctg = m_args[1];
            m_categories.push_back(m_parts_ctg);
            m_ctg_counts.push_back(0);
        }
        else if (strcmp(arg, "prefix") == 0)
        {
            CHECK_ARG(1);
            m_prefix = m_args[1];
        }
        else if (strcmp(arg, "rim") == 0)
        {
            CHECK_ARG(1);
            m_rim = (bool)atoi(m_args[1]);
        }
        else if (strcmp(arg, "sdir") == 0)
        {
            CHECK_ARG(1);
            m_sdir = m_args[1];
        }
        else if (strcmp(arg, "back_sdir") == 0)
        {
            CHECK_ARG(1);
            m_back_sdir = m_args[1];
        }
        else if (strcmp(arg, "weight") == 0)
        {
            CHECK_ARG(1);
            int tmp = atoi(m_args[1]);

            if (tmp <= 0)
            {
                fprintf(stderr, "Error (%s:%d): weight must be >= 1.\n",
                        list_file, line);
                return false;
            }

            m_weight = tmp;
        }
        else if (strcmp(arg, "domino") == 0)
        {
            CHECK_ARG(1);
            m_domino = atoi(m_args[1]);
        }
        else if (strcmp(arg, "shrink") == 0)
        {
            CHECK_ARG(1);
            m_shrink = (bool)atoi(m_args[1]);
        }
        else if (strcmp(arg, "start") == 0)
        {
            CHECK_NO_ARG(1);

            if (m_composing)
            {
                fprintf(stderr, "Error (%s:%d): already composing.\n",
                        list_file, line);
                return false;
            }
            m_composing = true;
            m_compose.unload();
        }
        else if (strcmp(arg, "startvalue") == 0)
        {
            CHECK_ARG(1);
            CHECK_NO_ARG(3);

            m_start_value = m_args[1];
            if (m_args.size() > 2)
            {
                m_start_value_module = m_args[2];
                char temp[1024];
                sprintf(temp, "tiledef-%s.h", m_args[2]);
                m_include.push_back(temp);
            }
        }
        else if (strcmp(arg, "pal") == 0)
        {
            // rgb (optional a) = rgb (optional a)
            tile_colour cols[2]  = { tile_colour::black, tile_colour::black };
            int col_idx = 0;
            int comp_idx = 0;

            for (size_t i = 1; i < m_args.size(); ++i)
            {
                if (strcmp(m_args[i], "="))
                {
                    if (comp_idx > 3)
                    {
                        fprintf(stderr, "Error (%s:%d): "
                                "Must be R G B (A) = R G B (A).\n",
                                list_file, line);
                        return false;
                    }

                    int val = atoi(m_args[i]);
                    if (val < 0 || val > 255)
                    {
                        fprintf(stderr,
                                "Error (%s:%d): Arg %d must be 0-255.\n",
                                list_file, line, (int)i);
                    }

                    cols[col_idx][comp_idx++] = static_cast<unsigned char>(val);
                }
                else if (col_idx > 0)
                {
                    fprintf(stderr,
                            "Error (%s:%d): Too many '=' characters.\n",
                            list_file, line);
                    return false;
                }
                else
                {
                    col_idx++;
                    comp_idx = 0;
                }
            }

            m_palette.push_back(palette_entry(cols[0], cols[1]));
        }
        else if (strcmp(arg, "hue") == 0)
        {
            CHECK_ARG(2);
            m_hues.push_back(int_pair(atoi(m_args[1]), atoi(m_args[2])));
        }
        else if (strcmp(arg, "alpha") == 0)
        {
            CHECK_ARG(1);
            m_alpha = ((double)atoi(m_args[1]))/255.0;
        }
        else if (strcmp(arg, "resetcol") == 0)
        {
            CHECK_NO_ARG(1);
            m_palette.clear();
            m_hues.clear();
            m_desat.clear();
            m_lum.clear();
            m_alpha = 0.0;
        }
        else if (strcmp(arg, "desat") == 0)
        {
            CHECK_ARG(1);
            CHECK_NO_ARG(2);

            m_desat.push_back(atoi(m_args[1]));
        }
        else if (strcmp(arg, "lum") == 0)
        {
            CHECK_ARG(2);
            CHECK_NO_ARG(3);

            m_lum.push_back(int_pair(atoi(m_args[1]), atoi(m_args[2])));
        }
        else if (strcmp(arg, "variation") == 0)
        {
            CHECK_ARG(2);
            CHECK_NO_ARG(3);

            int idx = m_page.find(m_args[1]);
            if (idx == -1)
            {
                fprintf(stderr, "Error (%s:%d): invalid tile name '%s'\n",
                        list_file, line, m_args[1]);
                return false;
            }

            int colour = str_to_colour(m_args[2]);
            if (colour == 0)
            {
                fprintf(stderr, "Error (%s:%d): invalid colour '%s'\n",
                        list_file, line, m_args[2]);
                return false;
            }

            m_variation_idx = idx;
            m_variation_col = colour;
        }
        else if (strcmp(arg, "repeat") == 0)
        {
            CHECK_ARG(1);

            int idx = m_page.find(m_args[1]);
            if (idx == -1)
            {
                fprintf(stderr, "Error (%s:%d): invalid tile name '%s'\n",
                        list_file, line, m_args[1]);
                return false;
            }

            int cnt = m_page.m_counts[idx];

            int old_w = 0;
            for (int i = 0; i < cnt; ++i)
            {
                tile img;
                img.copy(*m_page.m_tiles[idx + i]);
                recolour(img);
                m_weight = m_page.m_probs[idx + i] - old_w;
                old_w    = m_page.m_probs[idx + i];
                add_image(img, (i == 0 && m_args[2]) ? m_args[2] : nullptr);
            }

            if (m_args.size() > 2)
            {
                for (unsigned int i = 3; i < m_args.size(); ++i)
                {
                    // Add enums for additional values.
                    m_page.add_synonym(m_args[2], m_args[i]);
                }
            }
        }
        else if (strcmp(arg, "syn") == 0)
        {
            // Add a synonym without resetting the enum count.
            CHECK_ARG(1);

            for (unsigned int i = 1; i < m_args.size(); ++i)
                m_page.add_synonym(m_page.m_tiles.size() - 1, m_args[i]);
        }
        else
        {
            fprintf(stderr, "Error (%s:%d): unknown command '%%%s'\n",
                    list_file, line, arg);
            return false;
        }
    }
    else
    {
        if (m_composing)
        {
            fprintf(stderr, "Error (%s:%d): can't load while composing.\n",
                    list_file, line);
            return false;
        }

        tile img;

        if (!m_back.empty())
        {
            // compose
            if (!load_image(m_compose, arg, true))
            {
                fprintf(stderr, "Error (%s:%d): couldn't load image "
                                "'%s'.\n", list_file, line, arg);
                return false;
            }

            if (m_corpsify)
                m_compose.corpsify();

            if (m_texture)
                m_compose.texture(*m_texture);

            const unsigned int pseudo_rand = m_page.m_tiles.size() * 54321;
            tile *back = m_back[pseudo_rand % m_back.size()];
            img.copy(*back);
            if (!img.compose(m_compose))
            {
                fprintf(stderr, "Error (%s:%d): failed composing '%s'"
                                " onto back image '%s'.\n",
                        list_file, line, arg, back->filename().c_str());
                return false;
            }
        }
        else
        {
            if (!load_image(img, arg))
            {
                fprintf(stderr, "Error (%s:%d): couldn't load image "
                                "'%s'.\n", list_file, line, arg);
                return false;
            }

            if (m_corpsify)
                img.corpsify();

            if (m_texture)
                img.texture(*m_texture);
        }

        recolour(img);

        if (m_rim && !m_corpsify)
            img.add_rim(tile_colour::black);

        // Push tile onto tile page.
        add_image(img, m_args.size() > 1 ? m_args[1] : nullptr);

        for (unsigned int i = 2; i < m_args.size(); ++i)
        {
            // Add enums for additional values.
            m_page.add_synonym(m_args[1], m_args[i]);
        }
    }

    return true;
}

void tile_list_processor::add_image(tile &img, const char *enumname)
{
    tile *new_img = new tile(img, enumname, m_parts_ctg.c_str());
    new_img->set_shrink(m_shrink);

    m_page.m_tiles.push_back(new_img);
    m_page.m_counts.push_back(1);

    int weight = m_weight;
    if (enumname)
        m_last_enum = m_page.m_counts.size() - 1;
    else if (m_last_enum < m_page.m_counts.size() && !m_page.m_probs.empty())
    {
        m_page.m_counts[m_last_enum]++;
        weight += m_page.m_probs[m_page.m_probs.size() - 1];
    }

    m_page.m_base_tiles.push_back(m_last_enum);

    m_page.m_probs.push_back(weight);
    m_page.m_domino.push_back(m_domino);

    if (!m_categories.empty())
        m_ctg_counts[m_categories.size()-1]++;

    if (m_variation_idx != -1)
    {
        m_page.add_variation(m_last_enum, m_variation_idx, m_variation_col);
        m_variation_idx = -1;
    }
}

void tile_list_processor::add_abstracts(
    FILE *fp,
    const char *format,
    const vector<string> &lc_enum,
    const vector<string> &uc_max_enum,
    bool is_js)
{
    assert(lc_enum.size() == uc_max_enum.size());
    assert(!lc_enum.empty());

    size_t count = lc_enum.size();
    if (count == 1)
    {
        fprintf(fp, "%s", "    ");
        fprintf(fp, format, lc_enum[0].c_str());
        fprintf(fp, "%s", "\n");
        return;
    }

    for (size_t i = 0; i < count; ++i)
    {
        if (i != count - 1)
        {
            fprintf(fp,
                "    %s (idx < %s)\n"
                "    {\n"
                "        ",
                i != 0 ? "else if" : "if", uc_max_enum[i].c_str());
        }
        else
        {
            fprintf(fp,
                "    else\n"
                "    {\n"
                "        %s(idx < %s);\n"
                "        ",
                is_js ? "assert" : "ASSERT",
                uc_max_enum[i].c_str());
        }
        fprintf(fp, format, lc_enum[i].c_str());
        fprintf(fp, "%s", "\n    }\n");
    }
}

bool tile_list_processor::write_data(bool image, bool code)
{
    if (m_name == "")
    {
        fprintf(stderr, "%s", "Error: can't write data with no %%name specified.\n");
        return false;
    }

    if (!m_page.m_tiles.empty() && !m_abstract.empty())
    {
        fprintf(stderr, "%s", "Error: can't specify tiles when using %%abstract.\n");
        return false;
    }

    string lcname = m_name;
    string ucname = m_name;
    for (unsigned int i = 0; i < m_name.size(); i++)
    {
        lcname[i] = toalower(m_name[i]);
        ucname[i] = toaupper(m_name[i]);
    }
    string max = m_prefix;
    max += "_";
    max += ucname;
    max += "_MAX";

    string ctg_max = m_prefix;
    ctg_max += "_PART_MAX";

    // Write image page.
    {
        if (!m_page.place_images())
            return false;

        if (image)
        {
            char filename[1024];
            sprintf(filename, "%s.png", lcname.c_str());
            if (m_abstract.empty())
            {
                if (!m_page.write_image(filename))
                    return false;
            }
            else
            {
                // Write an empty file.
                char filename[1024];
                sprintf(filename, "%s.png", lcname.c_str());
                FILE *fp = fopen(filename, "w");
                if (!fp)
                {
                    fprintf(stderr, "Error: couldn't open '%s' for write.\n",
                            filename);
                    return false;
                }
                fclose(fp);
            }
        }
    }

    if (!code)
        return true;

    int *part_min = nullptr;

    // Write "tiledef-%name.h"
    {
        char filename[1024];
        sprintf(filename, "tiledef-%s.h", lcname.c_str());
        FILE *fp = fopen(filename, "w");

        if (!fp)
        {
            fprintf(stderr, "Error: couldn't open '%s' for write.\n", filename);
            return false;
        }

        if (!m_categories.empty())
        {
            part_min = new int[m_categories.size()];
            memset(part_min, 0, sizeof(int) * m_categories.size());
        }

        fprintf(fp, "// This file has been automatically generated.\n\n");
        fprintf(fp, "#pragma once\n\n");
        fprintf(fp, "#include \"tiledef_defines.h\"\n\n");

        for (const auto& include : m_include)
            fprintf(fp, "#include \"%s\"\n", include.c_str());

        fprintf(fp, "%s", "\n\n");

        fprintf(fp, "enum tile_%s_type\n{\n", lcname.c_str());

        string start_val = " = ";
        start_val += m_start_value;

        string old_enum_name = "";
        int count = 0;
        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            const string &parts_ctg = m_page.m_tiles[i]->parts_ctg();
            const int enumcount = m_page.m_tiles[i]->enumcount();

            if (enumcount == 0)
            {
                if (old_enum_name.empty())
                {
                    fprintf(fp, "    %s_%s_FILLER_%u%s,\n", m_prefix.c_str(),
                            ucname.c_str(), i, start_val.c_str());
                }
                else
                {
                    fprintf(fp, "    %s_%s_%d%s,\n", m_prefix.c_str(),
                            old_enum_name.c_str(), ++count, start_val.c_str());
                }
            }
            else if (parts_ctg.empty())
            {
                const string &enumname = m_page.m_tiles[i]->enumname(0);
                fprintf(fp, "    %s_%s%s,\n", m_prefix.c_str(),
                        enumname.c_str(), start_val.c_str());
                old_enum_name = enumname;
                count = 0;
            }
            else
            {
                const string &enumname = m_page.m_tiles[i]->enumname(0);
                fprintf(fp, "    %s_%s_%s%s,\n", m_prefix.c_str(),
                        parts_ctg.c_str(), enumname.c_str(), start_val.c_str());
                old_enum_name = enumname;
                count = 0;
            }

            for (int c = 1; c < enumcount; ++c)
            {
                const string &basename = m_page.m_tiles[i]->enumname(0);
                const string &enumname = m_page.m_tiles[i]->enumname(c);

                if (parts_ctg.empty())
                {
                    fprintf(fp, "    %s_%s = %s_%s,\n",
                        m_prefix.c_str(), enumname.c_str(),
                        m_prefix.c_str(), basename.c_str());
                }
                else
                {
                    fprintf(fp, "    %s_%s_%s = %s_%s_%s,\n",
                        m_prefix.c_str(), parts_ctg.c_str(), enumname.c_str(),
                        m_prefix.c_str(), parts_ctg.c_str(), basename.c_str());
                }
            }

            start_val = "";

            if (!parts_ctg.empty())
            {
                unsigned int idx;
                for (idx = 0; idx < m_categories.size(); idx++)
                    if (parts_ctg == m_categories[idx])
                        break;

                assert(idx < m_categories.size());
                if (part_min[idx] == 0)
                    part_min[idx] = i;
            }
        }

        if (m_abstract.empty())
            fprintf(fp, "    %s_%s_MAX\n};\n\n", m_prefix.c_str(), ucname.c_str());
        else
        {
            size_t last_idx = m_abstract.size() - 1;
            string max_enum = m_abstract[last_idx].second;
            max_enum += "_";
            max_enum += m_abstract[last_idx].first;
            max_enum += "_MAX";

            for (char& c : max_enum)
                c = toaupper(c);

            fprintf(fp, "    %s_%s_MAX = %s\n};\n\n", m_prefix.c_str(), ucname.c_str(), max_enum.c_str());
        }

        fprintf(fp, "unsigned int tile_%s_count(tileidx_t idx);\n", lcname.c_str());
        fprintf(fp, "tileidx_t tile_%s_basetile(tileidx_t idx);\n", lcname.c_str());
        fprintf(fp, "int tile_%s_probs(tileidx_t idx);\n",
                lcname.c_str());
        fprintf(fp, "int tile_%s_dominoes(tileidx_t idx);\n",
                lcname.c_str());
        fprintf(fp, "const char *tile_%s_name(tileidx_t idx);\n",
            lcname.c_str());
        fprintf(fp, "tile_info &tile_%s_info(tileidx_t idx);\n",
            lcname.c_str());
        fprintf(fp, "bool tile_%s_index(const char *str, tileidx_t *idx);\n",
            lcname.c_str());
        fprintf(fp, "bool tile_%s_equal(tileidx_t tile, tileidx_t idx);\n",
            lcname.c_str());
        fprintf(fp, "tileidx_t tile_%s_coloured(tileidx_t idx, int col);\n",
            lcname.c_str());

        if (!m_categories.empty())
        {
            fprintf(fp, "\nenum tile_%s_parts\n{\n", lcname.c_str());
            for (unsigned int i = 0; i < m_categories.size(); i++)
            {
                fprintf(fp, "    %s_PART_%s,\n", m_prefix.c_str(),
                        m_categories[i].c_str());
            }

            fprintf(fp, "    %s\n};\n\n", ctg_max.c_str());

            fprintf(fp, "extern unsigned int tile_%s_part_count[%s];\n",
                    lcname.c_str(), ctg_max.c_str());
            fprintf(fp, "extern tileidx_t tile_%s_part_start[%s];\n",
                    lcname.c_str(), ctg_max.c_str());
        }

        fclose(fp);
    }

    // write "tiledef-%name.cc"
    if (m_abstract.empty())
    {
        char filename[1024];
        sprintf(filename, "tiledef-%s.cc", lcname.c_str());
        FILE *fp = fopen(filename, "w");

        if (!fp)
        {
            fprintf(stderr, "Error: couldn't open '%s' for write.\n", filename);
            return false;
        }

        fprintf(fp, "// This file has been automatically generated.\n\n");
        fprintf(fp, "#include \"AppHdr.h\"\n");
        fprintf(fp, "#include \"libutil.h\"\n");
        fprintf(fp, "#include \"tiledef-%s.h\"\n\n", lcname.c_str());

        fprintf(fp, "static unsigned int _tile_%s_count[%s - %s] =\n{\n",
                lcname.c_str(), max.c_str(), m_start_value.c_str());
        for (unsigned int i = 0; i < m_page.m_counts.size(); i++)
            fprintf(fp, "    %u,\n", m_page.m_counts[i]);
        fprintf(fp, "};\n\n");

        fprintf(fp, "unsigned int tile_%s_count(tileidx_t idx)\n{\n", lcname.c_str());
        fprintf(fp, "    ASSERT_RANGE(idx, %s, %s);\n",
                m_start_value.c_str(), max.c_str());
        fprintf(fp, "    return _tile_%s_count[idx - %s];\n",
                lcname.c_str(), m_start_value.c_str());
        fprintf(fp, "}\n\n");

        fprintf(fp, "static tileidx_t _tile_%s_basetiles[%s - %s] =\n{\n",
                lcname.c_str(), max.c_str(), m_start_value.c_str());
        for (unsigned int i = 0; i < m_page.m_base_tiles.size(); i++)
            fprintf(fp, "    %u,\n", m_page.m_base_tiles[i]);
        fprintf(fp, "};\n\n");

        fprintf(fp, "tileidx_t tile_%s_basetile(tileidx_t idx)\n{\n", lcname.c_str());
        fprintf(fp, "    ASSERT_RANGE(idx, %s, %s);\n",
                m_start_value.c_str(), max.c_str());
        fprintf(fp, "    return _tile_%s_basetiles[idx - %s] + %s;\n",
                lcname.c_str(), m_start_value.c_str(), m_start_value.c_str());
        fprintf(fp, "}\n\n");

        fprintf(fp, "static int _tile_%s_probs[%s - %s] =\n{\n",
                lcname.c_str(), max.c_str(), m_start_value.c_str());
        for (unsigned int i = 0; i < m_page.m_probs.size(); i++)
            fprintf(fp, "    %u,\n", m_page.m_probs[i]);
        fprintf(fp, "};\n\n");

        fprintf(fp, "int tile_%s_probs(tileidx_t idx)\n{\n",
                    lcname.c_str());
        fprintf(fp, "    ASSERT_RANGE(idx, %s, %s);\n",
                m_start_value.c_str(), max.c_str());
        fprintf(fp, "    return _tile_%s_probs[idx - %s];\n",
                lcname.c_str(), m_start_value.c_str());
        fprintf(fp, "}\n\n");

        fprintf(fp, "static int _tile_%s_dominoes[%s - %s] =\n{\n",
                lcname.c_str(), max.c_str(), m_start_value.c_str());
        for (unsigned int i = 0; i < m_page.m_domino.size(); i++)
            fprintf(fp, "    %u,\n", m_page.m_domino[i]);
        fprintf(fp, "};\n\n");

        fprintf(fp, "int tile_%s_dominoes(tileidx_t idx)\n{\n",
                    lcname.c_str());
        fprintf(fp, "    ASSERT_RANGE(idx, %s, %s);\n",
                m_start_value.c_str(), max.c_str());
        fprintf(fp, "    return _tile_%s_dominoes[idx - %s];\n",
                lcname.c_str(), m_start_value.c_str());
        fprintf(fp, "}\n\n");

        fprintf(fp, "static const char *_tile_%s_name[%s - %s] =\n{\n",
                lcname.c_str(), max.c_str(), m_start_value.c_str());

        string old_enum_name = "";
        int count = 0;
        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            if (m_page.m_tiles[i]->enumcount() == 0)
            {
                if (old_enum_name.empty())
                    fprintf(fp, "    \"%s_FILLER_%u\",\n", ucname.c_str(), i);
                else
                {
                    fprintf(fp, "    \"%s_%d\",\n", old_enum_name.c_str(),
                            ++count);
                }
            }
            else
            {
                const string &enumname = m_page.m_tiles[i]->enumname(0);
                fprintf(fp, "    \"%s\",\n", enumname.c_str());
                old_enum_name = enumname;
                count = 0;
            }
        }
        fprintf(fp, "};\n\n");

        fprintf(fp, "const char *tile_%s_name(tileidx_t idx)\n{\n",
                lcname.c_str());
        fprintf(fp, "    ASSERT_RANGE(idx, %s, %s);\n",
                m_start_value.c_str(), max.c_str());
        fprintf(fp, "    return _tile_%s_name[idx - %s];\n",
                lcname.c_str(), m_start_value.c_str());
        fprintf(fp, "}\n\n");

        fprintf(fp, "static tile_info _tile_%s_info[%s - %s] =\n{\n",
                lcname.c_str(), max.c_str(), m_start_value.c_str());
        for (unsigned int i = 0; i < m_page.m_offsets.size(); i+=4)
        {
            fprintf(fp, "    tile_info(%d, %d, %d, %d, %d, %d, %d, %d),\n",
                    m_page.m_offsets[i+2], m_page.m_offsets[i+3],
                    m_page.m_offsets[i], m_page.m_offsets[i+1],
                    m_page.m_texcoords[i], m_page.m_texcoords[i+1],
                    m_page.m_texcoords[i+2], m_page.m_texcoords[i+3]);
        }
        fprintf(fp, "};\n\n");

        fprintf(fp, "tile_info &tile_%s_info(tileidx_t idx)\n{\n",
                lcname.c_str());
        fprintf(fp, "    ASSERT_RANGE(idx, %s, %s);\n",
                m_start_value.c_str(), max.c_str());
        fprintf(fp, "    return _tile_%s_info[idx - %s];\n",
                lcname.c_str(), m_start_value.c_str());
        fprintf(fp, "}\n\n");

        if (!m_categories.empty())
        {
            fprintf(fp, "unsigned int tile_%s_part_count[%s] =\n{\n",
                    lcname.c_str(), ctg_max.c_str());

            for (unsigned int i = 0; i < m_ctg_counts.size(); i++)
                fprintf(fp, "    %d,\n", m_ctg_counts[i]);

            fprintf(fp, "};\n\n");

            fprintf(fp, "tileidx_t tile_%s_part_start[%s] =\n{\n",
                    lcname.c_str(), ctg_max.c_str());

            for (unsigned int i = 0; i < m_categories.size(); i++)
                fprintf(fp, "    %d+%s,\n", part_min[i], m_start_value.c_str());

            fprintf(fp, "};\n\n");
        }

        fprintf(fp, "\ntypedef pair<const char*, tileidx_t> _name_pair;\n\n");

        fprintf(fp, "static _name_pair %s_name_pairs[] =\n"
                    "{\n", lcname.c_str());

        typedef map<string, int> sort_map;
        sort_map table;

        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            for (int c = 0; c < m_page.m_tiles[i]->enumcount(); ++c)
            {
                const string &enumname = m_page.m_tiles[i]->enumname(c);

                string lcenum = enumname;
                for (unsigned int c = 0; c < enumname.size(); c++)
                    lcenum[c] = toalower(enumname[c]);

                table.insert(sort_map::value_type(lcenum, i));
            }
        }

        sort_map::iterator itor;
        for (itor = table.begin(); itor != table.end(); itor++)
        {
            fprintf(fp, "    _name_pair(\"%s\", %d + %s),\n",
                    itor->first.c_str(), itor->second, m_start_value.c_str());
        }

        fprintf(fp, "};\n\n");

        fprintf(fp,
            "bool tile_%s_index(const char *str, tileidx_t *idx)\n"
            "{\n"
            "    ASSERT(str);\n"
            "    if (!str)\n"
            "        return false;\n"
            "\n"
            "    string lc = str;\n"
            "    for (unsigned int i = 0; i < lc.size(); i++)\n"
            "        lc[i] = toalower(lc[i]);\n"
            "\n"
            "    int num_pairs = sizeof(%s_name_pairs) / sizeof(%s_name_pairs[0]);\n"
            "    bool result = binary_search<const char *, tileidx_t>(\n"
            "       lc.c_str(), &%s_name_pairs[0], num_pairs, &strcmp, idx);\n"
            "    return result;\n"
            "}\n\n",
            lcname.c_str(), lcname.c_str(), lcname.c_str(), lcname.c_str());

        fprintf(fp,
            "bool tile_%s_equal(tileidx_t tile, tileidx_t idx)\n"
            "{\n"
            "    ASSERT_RANGE(tile, %s, %s);\n"
            "    return idx >= tile && idx < tile + tile_%s_count(tile);\n"
            "}\n\n",
            lcname.c_str(), m_start_value.c_str(), max.c_str(), lcname.c_str());

        fprintf(fp, "\ntypedef pair<tile_variation, tileidx_t> _colour_pair;\n\n");

        fprintf(fp,
            "static _colour_pair %s_colour_pairs[] =\n"
            "{\n"
            "    _colour_pair(tile_variation(0, 0), 0),\n",
            lcname.c_str());

        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            for (int c = 0; c < MAX_COLOUR; ++c)
            {
                int var;
                if (!m_page.m_tiles[i]->get_variation(c, var))
                    continue;

                fprintf(fp,
                    "    _colour_pair(tile_variation(%u + %s, %d), %d + %s),\n",
                    i, m_start_value.c_str(), c, var, m_start_value.c_str());
            }
        }

        fprintf(fp, "%s", "};\n\n");

        fprintf(fp,
            "tileidx_t tile_%s_coloured(tileidx_t idx, int col)\n"
            "{\n"
            "    int num_pairs = sizeof(%s_colour_pairs) / sizeof(%s_colour_pairs[0]);\n"
            "    tile_variation key(idx, col);\n"
            "    tileidx_t found;\n"
            "    bool result = binary_search<tile_variation, tileidx_t>(\n"
            "       key, &%s_colour_pairs[0], num_pairs,\n"
            "       &tile_variation::cmp, &found);\n"
            "    return (result ? found : idx);\n"
            "}\n\n",
            lcname.c_str(), lcname.c_str(), lcname.c_str(), lcname.c_str());

        fclose(fp);
    }
    else
    {
        if (m_abstract.size() == 1)
        {
            fprintf(stderr, "Error: <2 abstracts currently unsupported.\n");
            return false;
        }

        char filename[1024];
        sprintf(filename, "tiledef-%s.cc", lcname.c_str());
        FILE *fp = fopen(filename, "w");

        if (!fp)
        {
            fprintf(stderr, "Error: couldn't open '%s' for write.\n", filename);
            return false;
        }

        fprintf(fp, "// This file has been automatically generated.\n\n");
        fprintf(fp, "#include \"AppHdr.h\"\n");
        fprintf(fp, "#include \"tiledef-%s.h\"\n", lcname.c_str());
        fprintf(fp, "\n");

        vector<string> uc_max_enum;
        vector<string> lc_enum;
        for (const auto& abstract : m_abstract)
        {
            string max_enum = abstract.second;
            max_enum += "_";
            max_enum += abstract.first;
            max_enum += "_MAX";

            for (char& c : max_enum)
                c = toaupper(c);

            uc_max_enum.push_back(max_enum);

            lc_enum.push_back(abstract.first);
        }

        fprintf(fp, "unsigned int tile_%s_count(tileidx_t idx)\n{\n", lcname.c_str());
        add_abstracts(fp, "return (tile_%s_count(idx));", lc_enum, uc_max_enum);
        fprintf(fp, "}\n\n");

        fprintf(fp, "tileidx_t tile_%s_basetile(tileidx_t idx)\n{\n", lcname.c_str());
        add_abstracts(fp, "return (tile_%s_basetile(idx));", lc_enum, uc_max_enum);
        fprintf(fp, "}\n\n");

        fprintf(fp, "int tile_%s_probs(tileidx_t idx)\n{\n",
                    lcname.c_str());
        add_abstracts(fp, "return (tile_%s_probs(idx));", lc_enum, uc_max_enum);
        fprintf(fp, "}\n\n");

        fprintf(fp, "int tile_%s_dominoes(tileidx_t idx)\n{\n",
                    lcname.c_str());
        add_abstracts(fp, "return (tile_%s_dominoes(idx));", lc_enum, uc_max_enum);
        fprintf(fp, "}\n\n");

        fprintf(fp, "const char *tile_%s_name(tileidx_t idx)\n{\n",
                lcname.c_str());
        add_abstracts(fp, "return (tile_%s_name(idx));", lc_enum, uc_max_enum);
        fprintf(fp, "}\n\n");

        fprintf(fp, "tile_info &tile_%s_info(tileidx_t idx)\n{\n",
            lcname.c_str());
        add_abstracts(fp, "return (tile_%s_info(idx));", lc_enum, uc_max_enum);
        fprintf(fp, "}\n\n");

        fprintf(fp, "bool tile_%s_index(const char *str, tileidx_t *idx)\n{\n",
                lcname.c_str());
        for (const auto& str : lc_enum)
        {
            fprintf(fp,
                "    if (tile_%s_index(str, idx))\n"
                "        return true;\n",
                str.c_str());
        }
        fprintf(fp, "%s",
            "    return false;\n"
            "}\n\n");

        fprintf(fp, "bool tile_%s_equal(tileidx_t idx, tileidx_t cmp)\n{\n",
            lcname.c_str());
        add_abstracts(fp, "return (tile_%s_equal(idx, cmp));", lc_enum, uc_max_enum);
        fprintf(fp, "}\n\n");

        fprintf(fp, "tileidx_t tile_%s_coloured(tileidx_t idx, int col)\n{\n",
            lcname.c_str());
        add_abstracts(fp, "return (tile_%s_coloured(idx, col));", lc_enum, uc_max_enum);
        fprintf(fp, "}\n\n");

        fclose(fp);
    }

    // write "tile-%name.html"
    if (!m_page.m_tiles.empty())
    {
        char filename[1024];
        sprintf(filename, "tile-%s.html", lcname.c_str());
        FILE *fp = fopen(filename, "w");

        if (!fp)
        {
            fprintf(stderr, "Error: couldn't open '%s' for write.\n", filename);
            return false;
        }

        fprintf(fp, "<html><table>\n");

        fprintf(fp, "%s", "<tr><td>Image</td><td>Vault String</td><td>Enum</td><td>Path</td></tr>\n");

        int total_prob = 0;
        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            fprintf(fp, "<tr>");

            fprintf(fp, "<td><img src=\"%s\"/></td>",
                    m_page.m_tiles[i]->filename().c_str());

            if (m_page.m_tiles[i]->enumcount() == 0)
            {
                if (i == 0)
                    fprintf(fp, "<td></td><td></td>");
                else
                {
                    const float prob = m_page.m_probs[i] - m_page.m_probs[i-1];
                    const float perc = prob * 100 / (float)total_prob;
                    fprintf(fp, "<td>(%.1f%%)</td><td></td>", perc);
                }
            }
            else
            {
                string lcenum = m_page.m_tiles[i]->enumname(0);
                for (unsigned int c = 0; c < lcenum.size(); c++)
                    lcenum[c] = toalower(lcenum[c]);

                if (i == 0 || m_page.m_counts[i] == 1)
                    fprintf(fp, "<td>%s</td>", lcenum.c_str());
                else
                {
                    total_prob = m_page.m_probs[i + m_page.m_counts[i] - 1];
                    const float prob = m_page.m_probs[i];
                    const float perc = prob * 100 / (float)total_prob;
                    fprintf(fp, "<td>%s (%.1f%%)</td>", lcenum.c_str(), perc);
                }

                const string &parts_ctg = m_page.m_tiles[i]->parts_ctg();
                if (parts_ctg.empty())
                {
                    fprintf(fp, "<td>%s_%s</td>",
                            m_prefix.c_str(),
                            m_page.m_tiles[i]->enumname(0).c_str());
                }
                else
                {
                    fprintf(fp, "<td>%s_%s_%s</td>",
                            m_prefix.c_str(),
                            parts_ctg.c_str(),
                            m_page.m_tiles[i]->enumname(0).c_str());
                }
            }

            fprintf(fp, "<td>%s</td>", m_page.m_tiles[i]->filename().c_str());

            fprintf(fp, "</tr>\n");
        }

        fprintf(fp, "</table></html>\n");

        fclose(fp);
    }

    delete[] part_min;

    // Write "%name.d"
    if (m_abstract.empty())
    {
        char filename[1024];
        sprintf(filename, "%s.d", lcname.c_str());
        FILE *fp = fopen(filename, "w");

        if (!fp)
        {
            fprintf(stderr, "Error: couldn't open '%s' for write.\n", filename);
            return false;
        }

        if (!m_page.m_tiles.empty())
            fprintf(fp, "%s.png: \\\n", lcname.c_str());

        for (const auto& str : m_depends)
             fprintf(fp, "  %s \\\n", str.c_str());

        // Also generate empty dependencies for each file.
        // This way, if a file gets removed, the dependency file
        // won't complain about not having a rule to make the non-existent file.
        fprintf(fp, "%s", "\n\n");

        for (const auto& str : m_depends)
             fprintf(fp, "%s:\n", str.c_str());

        fclose(fp);
    }

    // write "tileinfo-%name.js"
    {
        char filename[1024];
        sprintf(filename, "tileinfo-%s.js", lcname.c_str());
        FILE *fp = fopen(filename, "w");

        if (!fp)
        {
            fprintf(stderr, "Error: couldn't open '%s' for write.\n", filename);
            return false;
        }

        if (m_abstract.size() == 0)
        {
            fprintf(fp, "define([");
            if (m_start_value_module.size() > 0)
                fprintf(fp, "\"./tileinfo-%s\"", m_start_value_module.c_str());
            fprintf(fp, "], function(m) {\n");
        }
        else
        {
            fprintf(fp, "define([\"jquery\",");
            for (const auto& abstract : m_abstract)
                fprintf(fp, "\"./tileinfo-%s\", ", abstract.first.c_str());
            fprintf(fp, "],\n       function ($, ");
            for (size_t i = 0; i < m_abstract.size(); ++i)
            {
                if (i < m_abstract.size() - 1)
                    fprintf(fp, "%s, ", m_abstract[i].first.c_str());
                else
                    fprintf(fp, "%s", m_abstract[i].first.c_str());
            }
            fprintf(fp, ") {\n");
        }
        fprintf(fp, "// This file has been automatically generated.\n\n");
        fprintf(fp, "var exports = {};\n");

        if (m_abstract.size() > 0)
        {
            for (const auto& abstract : m_abstract)
                fprintf(fp, "$.extend(exports, %s);\n", abstract.first.c_str());
        }

        if (m_start_value_module.size() > 0)
            fprintf(fp, "\nvar val = m.%s;\n", m_start_value.c_str());
        else
            fprintf(fp, "\nvar val = %s;\n", m_start_value.c_str());

        string old_enum_name = "";
        int count = 0;
        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            const string &parts_ctg = m_page.m_tiles[i]->parts_ctg();
            const int enumcount = m_page.m_tiles[i]->enumcount();

            if (enumcount == 0)
            {
                if (old_enum_name.empty())
                {
                    fprintf(fp, "exports.%s_FILLER_%u = val++;\n",
                            ucname.c_str(), i);
                }
                else
                {
                    fprintf(fp, "exports.%s_%d = val++;\n",
                            old_enum_name.c_str(), ++count);
                }
            }
            else if (parts_ctg.empty())
            {
                const string &enumname = m_page.m_tiles[i]->enumname(0);
                fprintf(fp, "exports.%s = val++;\n",
                        enumname.c_str());
                old_enum_name = enumname;
                count = 0;
            }
            else
            {
                const string &enumname = m_page.m_tiles[i]->enumname(0);
                fprintf(fp, "exports.%s_%s = val++;\n",
                        parts_ctg.c_str(), enumname.c_str());
                old_enum_name = enumname;
                count = 0;
            }

            for (int c = 1; c < enumcount; ++c)
            {
                const string &basename = m_page.m_tiles[i]->enumname(0);
                const string &enumname = m_page.m_tiles[i]->enumname(c);

                if (parts_ctg.empty())
                {
                    fprintf(fp, "val = exports.%s = exports.%s; val++;\n",
                            enumname.c_str(), basename.c_str());
                }
                else
                {
                    fprintf(fp, "val = exports.%s_%s = exports.%s_%s; val++;\n",
                            parts_ctg.c_str(), enumname.c_str(),
                            parts_ctg.c_str(), basename.c_str());
                }
            }
        }

        if (m_abstract.size() == 0)
        {
            fprintf(fp, "exports.%s_MAX = exports.%s_%s_MAX = val++;\n\n",
                    ucname.c_str(), m_prefix.c_str(), ucname.c_str());

            fprintf(fp, "var tile_info = [\n");
            for (unsigned int i = 0; i < m_page.m_offsets.size(); i+=4)
            {
                fprintf(fp, "  {w: %d, h: %d, ox: %d, oy: %d, sx: %d, sy: %d, ex: %d, ey: %d},\n",
                        m_page.m_offsets[i+2], m_page.m_offsets[i+3],
                        m_page.m_offsets[i], m_page.m_offsets[i+1],
                        m_page.m_texcoords[i], m_page.m_texcoords[i+1],
                        m_page.m_texcoords[i+2], m_page.m_texcoords[i+3]);
            }
            fprintf(fp, "];\n\n");

            fprintf(fp, "exports.get_tile_info = function (idx)\n{\n");
            fprintf(fp, "    return tile_info[idx - %s%s];\n",
                    m_start_value_module.size() > 0 ? "m." : "",
                    m_start_value.c_str());
            fprintf(fp, "};\n\n");

            fprintf(fp, "var _tile_count =\n[\n");
            for (unsigned int i = 0; i < m_page.m_counts.size(); i++)
                fprintf(fp, "    %u,\n", m_page.m_counts[i]);
            fprintf(fp, "];\n\n");

            fprintf(fp, "exports.tile_count = function (idx)\n{\n");
            fprintf(fp, "    return _tile_count[idx - %s%s];\n",
                    m_start_value_module.size() > 0 ? "m." : "",
                    m_start_value.c_str());
            fprintf(fp, "}\n\n");

            fprintf(fp, "var _basetiles =\n[\n");
            for (unsigned int i = 0; i < m_page.m_base_tiles.size(); i++)
                fprintf(fp, "    %u,\n", m_page.m_base_tiles[i]);
            fprintf(fp, "];\n\n");

            fprintf(fp, "exports.basetile = function (idx)\n{\n");
            fprintf(fp, "    return _basetiles[idx - %s%s] + %s%s;\n",
                    m_start_value_module.size() > 0 ? "m." : "", m_start_value.c_str(),
                    m_start_value_module.size() > 0 ? "m." : "", m_start_value.c_str());
            fprintf(fp, "};\n\n");

            fprintf(fp, "exports.get_img = function (idx) {\n");
            fprintf(fp, "    return \"%s\";\n", lcname.c_str());
            fprintf(fp, "};\n\n");
        }
        else
        {
            {
                size_t last_idx = m_abstract.size() - 1;

                string max_enum = "";
                max_enum += m_abstract[last_idx].first;
                max_enum += "_MAX";

                for (char& c : max_enum)
                    c = toaupper(c);

                fprintf(fp, "exports.%s_MAX = window.%s_%s_MAX = %s.%s;\n\n",
                        ucname.c_str(), m_prefix.c_str(), ucname.c_str()
                        , m_abstract[last_idx].first.c_str(),
                        max_enum.c_str());
            }

            vector<string> uc_max_enum;
            vector<string> lc_enum;
            for (const auto& abstract : m_abstract)
            {
                string max_enum = abstract.first;
                max_enum += "_MAX";
                for (char& c : max_enum)
                    c = toaupper(c);

                max_enum = abstract.first + "." + max_enum;

                uc_max_enum.push_back(max_enum);

                lc_enum.push_back(abstract.first);
            }

            fprintf(fp, "exports.get_tile_info = function (idx)\n{\n");
            add_abstracts(fp, "return (%s.get_tile_info(idx));", lc_enum, uc_max_enum, true);
            fprintf(fp, "};\n\n");

            fprintf(fp, "exports.tile_count = function (idx)\n{\n");
            add_abstracts(fp, "return (%s.tile_count(idx));", lc_enum, uc_max_enum, true);
            fprintf(fp, "};\n\n");

            fprintf(fp, "exports.basetile = function (idx)\n{\n");
            add_abstracts(fp, "return (%s.basetile(idx));", lc_enum, uc_max_enum, true);
            fprintf(fp, "};\n\n");

            fprintf(fp, "exports.get_img = function (idx) {\n");
            add_abstracts(fp, "return \"%s\";", lc_enum, uc_max_enum, true);
            fprintf(fp, "};\n\n");
        }

        fprintf(fp, "return exports;\n});\n");

        fclose(fp);
    }

    return true;
}

#include "tile_list_processor.h"

#include <cassert>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <map>

using namespace std;

tile_list_processor::tile_list_processor() :
    m_last_enum(~0),
    m_rim(false),
    m_corpsify(false),
    m_composing(false),
    m_shrink(true),
    m_prefix("TILE"),
    m_start_value("0")
{
}

tile_list_processor::~tile_list_processor()
{
    for (unsigned int i = 0; i < m_back.size(); i++)
    {
        delete m_back[i];
    }
    m_back.resize(0);
}

bool tile_list_processor::load_image(tile &img, const char *filename)
{
    assert(filename);

    char temp[1024];

    const int num_ext = 3;
    const char *ext[3] =
    {
        ".png",
        ".bmp",
        ""
    };

    if (m_sdir != "")
    {
        for (unsigned int e = 0; e < num_ext; e++)
        {
            sprintf(temp, "%s/%s%s", m_sdir.c_str(), filename, ext[e]);
            if (img.load(temp))
                return true;
        }
    }

    for (unsigned int e = 0; e < num_ext; e++)
    {
        sprintf(temp, "%s%s", filename, ext[e]);
        if (img.load(temp))
            return true;
    }

    return false;
}

bool tile_list_processor::process_list(const char *list_file)
{
    int line = 1;

    std::ifstream input(list_file);
    if (!input.is_open())
    {
        fprintf(stderr, "Error: couldn't open '%s' for read.\n", list_file);
        return false;
    }

    const size_t bufsize = 1024;
    char read_line[bufsize];
    bool success = true;
    while (!input.getline(read_line, bufsize).eof())
    {
        success &= process_line(read_line, list_file, line++);
    }

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

    std::vector<char *> m_args;
    m_args.push_back(arg);

    while (char *extra = strtok(NULL, delim))
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
            {
                delete m_back[i];
            }
            m_back.resize(0);

            if (strcmp(m_args[1], "none") == 0)
            {
                CHECK_NO_ARG(2);
                return true;
            }

            for (unsigned int i = 1; i < m_args.size(); i++)
            {
                tile *img = new tile();
                if (!load_image(*img, m_args[i]))
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

                if (!m_compose.compose(img))
                {
                    fprintf(stderr, "Error (%s:%d): failed composing '%s'"
                        " onto compose image.\n", list_file, line, m_args[1]);
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
            }
        }
        else if (strcmp(arg, "corpse") == 0)
        {
            CHECK_ARG(1);
            m_corpsify = (bool)atoi(m_args[1]);
        }
        else if (strcmp(arg, "end") == 0)
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

            if (m_back.size() > 0)
            {
                unsigned int psuedo_rand = m_page.m_tiles.size() * 54321;
                tile *back = m_back[psuedo_rand % m_back.size()];
                tile img(*back);
                if (!img.compose(m_compose))
                {
                    fprintf(stderr, "Error (%s:%d): failed composing '%s'"
                        " onto back image '%s'.\n", list_file, line,
                        arg, back->filename().c_str());
                    return false;
                }
                add_image(img, m_args.size() > 1 ? m_args[1] : NULL);
            }
            else
            {
                add_image(m_compose, m_args.size() > 1 ? m_args[1] : NULL);
            }

            m_compose.unload();
            m_composing = false;
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
                m_include = m_args[2];
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

        if (m_back.size() > 0)
        {
            // compose
            if (!load_image(m_compose, arg))
            {
                fprintf(stderr, "Error (%s:%d): couldn't load image "
                   "'%s'.\n", list_file, line, arg);
                return false;
            }

            if (m_corpsify)
                m_compose.corpsify();

            unsigned int psuedo_rand = m_page.m_tiles.size() * 54321;
            tile *back = m_back[psuedo_rand % m_back.size()];
            img.copy(*back);
            if (!img.compose(m_compose))
            {
                fprintf(stderr, "Error (%s:%d): failed composing '%s'"
                    " onto back image '%s'.\n", list_file, line,
                    arg, back->filename().c_str());
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
        }

        if (m_rim && !m_corpsify)
            img.add_rim(tile_colour::black);

        // push tile onto tile page
        add_image(img, m_args.size() > 1 ? m_args[1] : NULL);
    }

    return true;
}

void tile_list_processor::add_image(tile &img, const char *enumname)
{
    tile *new_img = new tile(img, enumname, m_parts_ctg.c_str());
    new_img->set_shrink(m_shrink);

    m_page.m_tiles.push_back(new_img);
    m_page.m_counts.push_back(1);

    if (enumname)
        m_last_enum = m_page.m_counts.size() - 1;
    else if (m_last_enum < m_page.m_counts.size())
        m_page.m_counts[m_last_enum]++;

    if (m_categories.size() > 0)
    {
        m_ctg_counts[m_categories.size()-1]++;
    }
}

bool tile_list_processor::write_data()
{
    if (m_name == "")
    {
        fprintf(stderr, "Error: can't write data with no %%name specified.\n");
        return false;
    }

    std::string lcname = m_name;
    std::string ucname = m_name;
    for (unsigned int i = 0; i < m_name.size(); i++)
    {
        lcname[i] = std::tolower(m_name[i]);
        ucname[i] = std::toupper(m_name[i]);
    }
    std::string max = m_prefix;
    max += "_";
    max += ucname;
    max += "_MAX";

    std::string ctg_max = m_prefix;
    ctg_max += "_PART_MAX";

    // write image page
    {
        if (!m_page.place_images())
            return false;

        char filename[1024];
        sprintf(filename, "%s.png", lcname.c_str());
        if (!m_page.write_image(filename))
            return false;
    }

    int *part_min = NULL;

    // write "tiledef-%name.h"
    {
        char filename[1024];
        sprintf(filename, "tiledef-%s.h", lcname.c_str());
        FILE *fp = fopen(filename, "w");
        
        if (!fp)
        {
            fprintf(stderr, "Error: couldn't open '%s' for write.\n", filename);
            return false;
        }

        if (m_categories.size() > 0)
        {
            part_min = new int[m_categories.size()];
            memset(part_min, 0, sizeof(int) * m_categories.size());
        }

        fprintf(fp, "// This file has been automatically generated.\n\n");
        fprintf(fp, "#ifndef TILEDEF_%s_H\n#define TILEDEF_%s_H\n\n",
            ucname.c_str(), ucname.c_str());
        fprintf(fp, "#include \"tiledef_defines.h\"\n\n");

        if (!m_include.empty())
        {
            fprintf(fp, "#include \"%s\"\n\n", m_include.c_str());
        }

        fprintf(fp, "enum tile_%s_type\n{\n", lcname.c_str());

        std::string start_val = " = ";
        start_val += m_start_value;

        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            const std::string &enumname = m_page.m_tiles[i]->enumname();
            const std::string &parts_ctg = m_page.m_tiles[i]->parts_ctg();
            if (enumname.empty())
            {
                fprintf(fp, "    %s_%s_FILLER_%d%s,\n", m_prefix.c_str(), 
                    ucname.c_str(), i, start_val.c_str());
            }
            else if (parts_ctg.empty())
            {
                fprintf(fp, "    %s_%s%s,\n", m_prefix.c_str(),
                    enumname.c_str(), start_val.c_str());
            }
            else
            {
                fprintf(fp, "    %s_%s_%s%s,\n", m_prefix.c_str(),
                    parts_ctg.c_str(), enumname.c_str(), start_val.c_str());
            }

            start_val = "";

            if (!parts_ctg.empty())
            {
                int idx;
                for (idx = 0; idx < m_categories.size(); idx++)
                {
                    if (parts_ctg == m_categories[idx])
                        break;
                }
                assert(idx < m_categories.size());
                if (part_min[idx] == 0)
                    part_min[idx] = i;
            }
        }

        fprintf(fp, "    %s_%s_MAX\n};\n\n", m_prefix.c_str(), ucname.c_str());

        fprintf(fp, "int tile_%s_count(unsigned int idx);\n", lcname.c_str());
        fprintf(fp, "const char *tile_%s_name(unsigned int idx);\n",
            lcname.c_str());
        fprintf(fp, "tile_info &tile_%s_info(unsigned int idx);\n",
            lcname.c_str());
        fprintf(fp, "bool tile_%s_index(const char *str, unsigned int &idx);\n",
            lcname.c_str());
        fprintf(fp, "bool tile_%s_equal(unsigned int tile, unsigned int idx);\n",
            lcname.c_str());

        if (m_categories.size() > 0)
        {
            fprintf(fp, "\nenum tile_%s_parts\n{\n", lcname.c_str());
            for (unsigned int i = 0; i < m_categories.size(); i++)
            {
                fprintf(fp, "    %s_PART_%s,\n", m_prefix.c_str(),
                    m_categories[i].c_str());
            }

            fprintf(fp, "    %s\n};\n\n", ctg_max.c_str());

            fprintf(fp, "extern int tile_%s_part_count[%s];\n",
                lcname.c_str(), ctg_max.c_str());
            fprintf(fp, "extern int tile_%s_part_start[%s];\n",
                lcname.c_str(), ctg_max.c_str());
        }

        fprintf(fp, "\n#endif\n\n");

        fclose(fp);
    }

    // write "tiledef-%name.cc"
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
        fprintf(fp, "#include \"tiledef-%s.h\"\n\n", lcname.c_str());
        fprintf(fp, "#include <string>\n");
        fprintf(fp, "#include <cstring>\n");
        fprintf(fp, "#include <cassert>\n");
        fprintf(fp, "using namespace std;\n\n");

        fprintf(fp, "int _tile_%s_count[%s - %s] =\n{\n",
            lcname.c_str(), max.c_str(), m_start_value.c_str());
        for (unsigned int i = 0; i < m_page.m_counts.size(); i++)
            fprintf(fp, "    %d,\n", m_page.m_counts[i]);
        fprintf(fp, "};\n\n");

        fprintf(fp, "int tile_%s_count(unsigned int idx)\n{\n", lcname.c_str());
        fprintf(fp, "    assert(idx >= %s && idx < %s);\n",
            m_start_value.c_str(), max.c_str());
        fprintf(fp, "    return _tile_%s_count[idx - %s];\n",
            lcname.c_str(), m_start_value.c_str());
        fprintf(fp, "}\n\n");

        fprintf(fp, "const char *_tile_%s_name[%s - %s] =\n{\n",
            lcname.c_str(), max.c_str(), m_start_value.c_str());
        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            const std::string &enumname = m_page.m_tiles[i]->enumname();
            if (enumname.empty())
                fprintf(fp, "    \"%s_FILLER_%d\",\n", ucname.c_str(), i);
            else
                fprintf(fp, "    \"%s\",\n", enumname.c_str());
        }
        fprintf(fp, "};\n\n");

        fprintf(fp, "const char *tile_%s_name(unsigned int idx)\n{\n",
            lcname.c_str());
        fprintf(fp, "    assert(idx >= %s && idx < %s);\n",
            m_start_value.c_str(), max.c_str());
        fprintf(fp, "    return _tile_%s_name[idx - %s];\n",
            lcname.c_str(), m_start_value.c_str());
        fprintf(fp, "}\n\n");

        fprintf(fp, "tile_info _tile_%s_info[%s - %s] =\n{\n",
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

        fprintf(fp, "tile_info &tile_%s_info(unsigned int idx)\n{\n",
            lcname.c_str());
        fprintf(fp, "    assert(idx >= %s && idx < %s);\n",
            m_start_value.c_str(), max.c_str());
        fprintf(fp, "    return _tile_%s_info[idx - %s];\n",
            lcname.c_str(), m_start_value.c_str());
        fprintf(fp, "}\n\n");

        if (m_categories.size() > 0)
        {
            fprintf(fp, "int tile_%s_part_count[%s] =\n{\n",
                lcname.c_str(), ctg_max.c_str());

            for (int i = 0; i < m_ctg_counts.size(); i++)
            {
                fprintf(fp, "    %d,\n", m_ctg_counts[i]);
            }

            fprintf(fp, "};\n\n");

            fprintf(fp, "int tile_%s_part_start[%s] =\n{\n",
                lcname.c_str(), ctg_max.c_str());

            for (int i = 0; i < m_categories.size(); i++)
            {
                fprintf(fp, "    %d+%s,\n", part_min[i], m_start_value.c_str());
            }

            fprintf(fp, "};\n\n");
        }

        fprintf(fp, "\ntypedef std::pair<const char*, int> _tile_pair;\n\n");

        fprintf(fp,
            "_tile_pair %s_map_pairs[] =\n"
            "{\n",
            lcname.c_str());

        typedef std::map<std::string, int> sort_map;
        sort_map table;

        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            const std::string &enumname = m_page.m_tiles[i]->enumname();
            // Filler can't be looked up.
            if (enumname.empty())
                continue;

            std::string lcenum = enumname;
            for (unsigned int c = 0; c < enumname.size(); c++)
                lcenum[c] = std::tolower(enumname[c]);

            table.insert(sort_map::value_type(lcenum, i));
        }

        sort_map::iterator itor;
        for (itor = table.begin(); itor != table.end(); itor++)
        {
            fprintf(fp, "    _tile_pair(\"%s\", %d + %s),\n",
                itor->first.c_str(), itor->second, m_start_value.c_str());
        }

        fprintf(fp, "};\n\n");

        fprintf(fp,
            "bool tile_%s_index(const char *str, unsigned int &idx)\n"
            "{\n"
            "    assert(str);\n"
            "    if (!str)\n"
            "        return false;\n"
            "\n"
            "    string lc = str;\n"
            "    for (unsigned int i = 0; i < lc.size(); i++)\n"
            "        lc[i] = tolower(lc[i]);\n"
            "\n"
            "    int num_pairs = sizeof(%s_map_pairs) / sizeof(%s_map_pairs[0]);\n"
            "\n"
            "    int first = 0;\n"
            "    int last = num_pairs - 1;\n"
            "\n"
            "    do\n"
            "    {\n"
            "        int half = (last - first) / 2 + first;\n"
            "        int cmp = strcmp(str, %s_map_pairs[half].first);\n"
            "        if (cmp < 0)\n"
            "            last = half - 1;\n"
            "        else if (cmp > 0)\n"
            "            first = half + 1;\n"
            "        else\n"
            "        {\n"
            "            idx = %s_map_pairs[half].second;\n"
            "            return true;\n"
            "        }\n" "\n"
            "    } while (first <= last);\n"
            "\n"
            "    return false;\n"
            "}\n",
            lcname.c_str(), lcname.c_str(), lcname.c_str(), lcname.c_str(), lcname.c_str());

        fprintf(fp,
            "bool tile_%s_equal(unsigned int tile, unsigned int idx)\n"
            "{\n"
            "    assert(tile >= %s && tile < %s);\n"
            "    return (idx >= tile && idx < tile + tile_%s_count(tile));\n"
            "}\n\n",
            lcname.c_str(), m_start_value.c_str(), max.c_str(), lcname.c_str());

        fclose(fp);
    }

    // write "tile-%name.html"
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

        for (unsigned int i = 0; i < m_page.m_tiles.size(); i++)
        {
            fprintf(fp, "<tr>");
            
            fprintf(fp, "<td><img src=\"%s\"/></td>",
                m_page.m_tiles[i]->filename().c_str());

            std::string lcenum = m_page.m_tiles[i]->enumname();
            for (unsigned int c = 0; c < lcenum.size(); c++)
                lcenum[c] = std::tolower(lcenum[c]);

            fprintf(fp, "<td>%s</td>", lcenum.c_str());

            const std::string &parts_ctg = m_page.m_tiles[i]->parts_ctg();
            if (m_page.m_tiles[i]->enumname().empty())
            {
                fprintf(fp, "<td></td>");
            }
            else if (parts_ctg.empty())
            {
                fprintf(fp, "<td>%s_%s</td>",
                    m_prefix.c_str(), m_page.m_tiles[i]->enumname().c_str());
            }
            else
            {
                fprintf(fp, "<td>%s_%s_%s</td>",
                    m_prefix.c_str(),
                    parts_ctg.c_str(),
                    m_page.m_tiles[i]->enumname().c_str());
            }

            fprintf(fp, "<td>%s</td>", m_page.m_tiles[i]->filename().c_str());

            fprintf(fp, "</tr>\n");
        }

        fprintf(fp, "</table></html>\n");

        fclose(fp);
    }

    delete[] part_min;

    return true;
}

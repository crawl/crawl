/*
 *  File:       format.h
 *  Created by: haranp on Sat Feb 17 13:35:54 2007 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef __FORMAT_H__
#define __FORMAT_H__

#include <string>
#include <vector>

#include "externs.h"

// Definitions for formatted_string

enum fs_op_type
{
    FSOP_COLOUR,
    FSOP_CURSOR,
    FSOP_TEXT
};

class formatted_string
{
public:
    formatted_string(int init_colour = 0);
    explicit formatted_string(const std::string &s, int init_colour = 0);

    operator std::string() const;
    void display(int start = 0, int end = -1) const;
    std::string tostring(int start = 0, int end = -1) const;
    std::string to_colour_string() const;

    void cprintf(const char *s, ...);
    void cprintf(const std::string &s);
    void cgotoxy(int x, int y);
    void movexy(int delta_x, int delta_y);
    void add_glyph(const monsters *mons);
    void add_glyph(const item_def *item);
    void textcolor(int color);
    formatted_string substr(size_t index, size_t length=std::string::npos) const;

    void clear();

    void swap(formatted_string& other);

    std::string::size_type length() const;
    std::string html_dump() const;

    bool operator < ( const formatted_string &other ) const;
    const formatted_string &operator += (const formatted_string &other);
    char &operator [] (size_t idx);

public:
    static formatted_string parse_string(
            const std::string &s,
            bool  eot_ends_format = true,
            bool (*process_tag)(const std::string &tag) = NULL,
            int main_colour = LIGHTGREY );

    static void parse_string_to_multiple(
            const std::string &s,
            std::vector<formatted_string> &out);

    static formatted_string parse_block(
            const std::string &s,
            bool  eot_ends_format = true,
            bool (*process_tag)(const std::string &tag) = NULL );

    static int get_colour(const std::string &tag);

private:
    int find_last_colour() const;

    static void parse_string1(
            const std::string &s,
            formatted_string &fs,
            std::vector<int> &colour_stack,
            bool (*process_tag)(const std::string &tag));

public:
    struct fs_op
    {
        fs_op_type type;
        int x, y;
        bool relative;
        std::string text;

        fs_op(int color)
            : type(FSOP_COLOUR), x(color), y(-1), relative(false), text()
        {
        }

        fs_op(int cx, int cy, bool rel = false)
            : type(FSOP_CURSOR), x(cx), y(cy), relative(rel), text()
        {
        }

        fs_op(const std::string &s)
            : type(FSOP_TEXT), x(-1), y(-1), relative(false), text(s)
        {
        }

        operator fs_op_type () const
        {
            return type;
        }
        void display() const;
    };

    typedef std::vector<fs_op> oplist;
    oplist ops;
};

int count_linebreaks(const formatted_string& fs);

int tagged_string_tag_length(const std::string& s);
int tagged_string_printable_length(const std::string& s);
std::string tagged_string_substr(const std::string& s, int start, int end);

#endif

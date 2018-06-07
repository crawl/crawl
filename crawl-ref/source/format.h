#pragma once

#include <string>
#include <vector>

// Definitions for formatted_string

enum fs_op_type
{
    FSOP_COLOUR,
    FSOP_TEXT,
};

class formatted_string
{
public:
    formatted_string(int init_colour = 0);
    explicit formatted_string(const string &s, int init_colour = 0);

    operator string() const;
    void display(int start = 0, int end = -1) const;
    string tostring(int start = 0, int end = -1) const;
    string to_colour_string() const;

    void cprintf(PRINTF(1, ));
    void cprintf(const string &s);
    void add_glyph(cglyph_t g);
    void textcolour(int colour);
    formatted_string chop(int length) const;
    formatted_string chop_bytes(int length) const;
    void del_char();
    void all_caps();
    void capitalise();
    void filter_lang();

    void clear();
    bool empty();

    void swap(formatted_string& other);

    int width() const;
    string html_dump() const;

    bool operator < (const formatted_string &other) const;
    const formatted_string &operator += (const formatted_string &other);
    char &operator [] (size_t idx);

public:
    static formatted_string parse_string(
            const string &s,
            int main_colour = LIGHTGREY);

    static void parse_string_to_multiple(const string &s,
                                         vector<formatted_string> &out,
                                         int wrap_col = 0);


private:
    static int get_colour(const string &tag);
    int find_last_colour() const;

    static void parse_string1(const string &s, formatted_string &fs,
                              vector<int> &colour_stack);

public:
    struct fs_op
    {
        fs_op_type type;
        int x, y;
        bool relative;
        string text;

        fs_op(int colour)
            : type(FSOP_COLOUR), x(colour), y(-1), relative(false), text()
        {
        }

        fs_op(const string &s)
            : type(FSOP_TEXT), x(-1), y(-1), relative(false), text(s)
        {
        }

        operator fs_op_type () const
        {
            return type;
        }
        void display() const;
    };

    typedef vector<fs_op> oplist;
    oplist ops;
};

int count_linebreaks(const formatted_string& fs);

int tagged_string_tag_length(const string& s);
int printed_width(const string& s);

void display_tagged_block(const string& s);

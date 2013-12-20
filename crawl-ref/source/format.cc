#include <limits.h>

#include "AppHdr.h"

#include "colour.h"
#include "format.h"
#include "libutil.h"
#include "lang-fake.h"
#include "unicode.h"
#include "viewchar.h"

formatted_string::formatted_string(int init_colour)
    : ops()
{
    if (init_colour)
        textcolor(init_colour);
}

formatted_string::formatted_string(const string &s, int init_colour)
    : ops()
{
    if (init_colour)
        textcolor(init_colour);
    cprintf(s);
}

int formatted_string::get_colour(const string &tag)
{
    if (tag == "h")
        return YELLOW;

    if (tag == "w")
        return WHITE;

    const int colour = str_to_colour(tag);
    return colour != -1? colour : LIGHTGREY;
}

// Display a formatted string without printing literal \n.
// This is important if the text is not supposed
// to clobber existing text to the right of the lines being displayed
// (some of the tutorial messages need this).
void display_tagged_block(const string &s)
{
    vector<formatted_string> lines;
    formatted_string::parse_string_to_multiple(s, lines);

    int x = wherex();
    int y = wherey();
    const unsigned int max_y = cgetsize(GOTO_CRT).y;
    const int size = min<unsigned int>(lines.size(), max_y - y + 1);
    for (int i = 0; i < size; ++i)
    {
        cgotoxy(x, y);
        lines[i].display();
        y++;
    }
}

formatted_string formatted_string::parse_string(const string &s,
                                                bool eot_ends_format,
                                                bool (*process)(const string &tag),
                                                int main_colour)
{
    // main_colour will usually be LIGHTGREY (default).
    vector<int> colour_stack;
    colour_stack.push_back(main_colour);

    formatted_string fs;

    parse_string1(s, fs, colour_stack, process);
    if (eot_ends_format)
    {
        if (colour_stack.back() != colour_stack.front())
            fs.textcolor(colour_stack.front());
    }
    return fs;
}

// Parses a formatted string in much the same way as parse_string, but
// handles \n by creating a new formatted_string.
void formatted_string::parse_string_to_multiple(const string &s,
                                                vector<formatted_string> &out)
{
    vector<int> colour_stack;
    colour_stack.push_back(LIGHTGREY);

    vector<string> lines = split_string("\n", s, false, true);

    for (int i = 0, size = lines.size(); i < size; ++i)
    {
        out.push_back(formatted_string());
        formatted_string& fs = out.back();
        fs.textcolor(colour_stack.back());
        parse_string1(lines[i], fs, colour_stack, NULL);
        if (colour_stack.back() != colour_stack.front())
            fs.textcolor(colour_stack.front());
    }
}

// Helper for the other parse_ methods.
void formatted_string::parse_string1(const string &s, formatted_string &fs,
                                     vector<int> &colour_stack,
                                     bool (*process)(const string &tag))
{
    // FIXME: This is a lame mess, just good enough for the task on hand
    // (keyboard help).
    string::size_type tag    = string::npos;
    string::size_type length = s.length();

    string currs;
    bool masked = false;

    for (tag = 0; tag < length; ++tag)
    {
        bool revert_colour = false;
        string::size_type endpos = string::npos;

        // Break string up if it gets too big.
        if (currs.size() >= 999)
        {
            // Break the string at the end of a line, if possible, so
            // that none of the broken string ends up overwritten.
            string::size_type bound = currs.rfind("\n", 999);
            if (bound != endpos)
                bound++;
            else
                bound = 999;

            fs.cprintf(currs.substr(0, bound));
            if (currs.size() > bound)
                currs = currs.substr(bound);
            else
                currs.clear();
            tag--;
            continue;
        }

        if (s[tag] != '<' || tag >= length - 1)
        {
            if (!masked)
                currs += s[tag];
            continue;
        }

        // Is this a << escape?
        if (s[tag + 1] == '<')
        {
            if (!masked)
                currs += s[tag];
            tag++;
            continue;
        }

        endpos = s.find('>', tag + 1);
        // No closing >?
        if (endpos == string::npos)
        {
            if (!masked)
                currs += s[tag];
            continue;
        }

        string tagtext = s.substr(tag + 1, endpos - tag - 1);
        if (tagtext.empty() || tagtext == "/")
        {
            if (!masked)
                currs += s[tag];
            continue;
        }

        if (tagtext[0] == '/')
        {
            revert_colour = true;
            tagtext = tagtext.substr(1);
            tag++;
        }

        if (tagtext[0] == '?')
        {
            if (tagtext.length() == 1)
                masked = false;
            else if (process && !process(tagtext.substr(1)))
                masked = true;

            tag += tagtext.length() + 1;
            continue;
        }

        if (!currs.empty())
        {
            fs.cprintf(currs);
            currs.clear();
        }

        if (revert_colour)
        {
            colour_stack.pop_back();
            if (colour_stack.size() < 1)
                die("Stack underflow in string \"%s\"", s.c_str());
        }
        else
            colour_stack.push_back(get_colour(tagtext));

        // fs.cprintf("%d%d", colour_stack.size(), colour_stack.back());
        fs.textcolor(colour_stack.back());

        tag += tagtext.length() + 1;
    }
    if (currs.length())
        fs.cprintf(currs);
}

formatted_string::operator string() const
{
    string s;
    for (unsigned i = 0, size = ops.size(); i < size; ++i)
    {
        if (ops[i] == FSOP_TEXT)
            s += ops[i].text;
    }
    return s;
}

static void _replace_all_in_string(string& s, const string& search,
                                   const string& replace)
{
    string::size_type pos = 0;
    while ((pos = s.find(search, pos)) != string::npos)
    {
        s.replace(pos, search.size(), replace);
        pos += replace.size();
    }
}

string formatted_string::html_dump() const
{
    string s;
    for (unsigned i = 0; i < ops.size(); ++i)
    {
        string tmp;
        switch (ops[i].type)
        {
        case FSOP_TEXT:
            tmp = ops[i].text;
            // (very) crude HTMLification
            _replace_all_in_string(tmp, "&", "&amp;");
            _replace_all_in_string(tmp, " ", "&nbsp;");
            _replace_all_in_string(tmp, "<", "&lt;");
            _replace_all_in_string(tmp, ">", "&gt;");
            _replace_all_in_string(tmp, "\n", "<br>");
            s += tmp;
            break;
        case FSOP_COLOUR:
            s += "<font color=";
            s += colour_to_str(ops[i].x);
            s += ">";
            break;
        }
    }
    return s;
}

bool formatted_string::operator < (const formatted_string &other) const
{
    return string(*this) < string(other);
}

const formatted_string &
formatted_string::operator += (const formatted_string &other)
{
    ops.insert(ops.end(), other.ops.begin(), other.ops.end());
    return *this;
}

int formatted_string::width() const
{
    // Just add up the individual string lengths.
    int len = 0;
    for (unsigned i = 0, size = ops.size(); i < size; ++i)
        if (ops[i] == FSOP_TEXT)
            len += strwidth(ops[i].text);
    return len;
}

static inline void cap(int &i, int max)
{
    if (i < 0 && -i <= max)
        i += max;
    if (i >= max)
        i = max - 1;
    if (i < 0)
        i = 0;
}

char &formatted_string::operator [] (size_t idx)
{
    size_t rel_idx = idx;
    int size = ops.size();
    for (int i = 0; i < size; ++i)
    {
        if (ops[i] != FSOP_TEXT)
            continue;

        size_t len = ops[i].text.length();
        if (rel_idx >= len)
            rel_idx -= len;
        else
            return ops[i].text[rel_idx];
    }
    die("Invalid index");
}

string formatted_string::tostring(int s, int e) const
{
    string st;

    int size = ops.size();
    cap(s, size);
    cap(e, size);

    for (int i = s; i <= e && i < size; ++i)
    {
        if (ops[i] == FSOP_TEXT)
            st += ops[i].text;
    }
    return st;
}

string formatted_string::to_colour_string() const
{
    string st;
    const int size = ops.size();
    for (int i = 0; i < size; ++i)
    {
        if (ops[i] == FSOP_TEXT)
        {
            // gotta double up those '<' chars ...
            size_t start = st.size();
            st += ops[i].text;

            while (true)
            {
                const size_t left_angle = st.find('<', start);
                if (left_angle == string::npos)
                    break;

                st.insert(left_angle, "<");
                start = left_angle + 2;
            }
        }
        else if (ops[i] == FSOP_COLOUR)
        {
            st += "<";
            st += colour_to_str(ops[i].x);
            st += ">";
        }
    }

    return st;
}

void formatted_string::display(int s, int e) const
{
    int size = ops.size();
    if (!size)
        return;

    cap(s, size);
    cap(e, size);

    for (int i = s; i <= e && i < size; ++i)
        ops[i].display();
}

int formatted_string::find_last_colour() const
{
    if (!ops.empty())
    {
        for (int i = ops.size() - 1; i >= 0; --i)
            if (ops[i].type == FSOP_COLOUR)
                return ops[i].x;
    }
    return LIGHTGREY;
}

formatted_string formatted_string::chop(int length) const
{
    formatted_string result;
    for (unsigned int i = 0; i<ops.size(); i++)
    {
        const fs_op& op = ops[i];
        if (op.type == FSOP_TEXT)
        {
            result.ops.push_back(op);
            string& new_string = result.ops[result.ops.size()-1].text;
            int w = strwidth(new_string);
            if (w > length)
                new_string = chop_string(new_string, length, false);
            length -= w;
            if (length <= 0)
                break;
        }
        else
            result.ops.push_back(op);
    }

    return result;
}

void formatted_string::del_char()
{
    for (oplist::iterator i = ops.begin(); i != ops.end(); ++i)
    {
        if (i->type != FSOP_TEXT)
            continue;
        switch (strwidth(i->text))
        {
        case 0: // shouldn't happen
            continue;
        case 1:
            ops.erase(i);
            return;
        }
        i->text = next_glyph((char*)i->text.c_str());
        return;
    }
}

void formatted_string::add_glyph(cglyph_t g)
{
    const int last_col = find_last_colour();
    this->textcolor(g.col);
    this->cprintf("%s", stringize_glyph(g.ch).c_str());
    this->textcolor(last_col);
}

void formatted_string::textcolor(int color)
{
    if (!ops.empty() && ops[ ops.size() - 1 ].type == FSOP_COLOUR)
        ops.erase(ops.end() - 1);

    ops.push_back(color);
}

void formatted_string::clear()
{
    ops.clear();
}

bool formatted_string::empty()
{
    return ops.empty();
}

void formatted_string::cprintf(const char *s, ...)
{
    va_list args;
    va_start(args, s);
    cprintf(vmake_stringf(s, args));
    va_end(args);
}

void formatted_string::cprintf(const string &s)
{
    ops.push_back(s);
}

void formatted_string::fs_op::display() const
{
    switch (type)
    {
    case FSOP_COLOUR:
        ::textcolor(x);
        break;
    case FSOP_TEXT:
        ::cprintf("%s", text.c_str());
        break;
    }
}

void formatted_string::swap(formatted_string& other)
{
    ops.swap(other.ops);
}

void formatted_string::all_caps()
{
    for (unsigned int i = 0; i < ops.size(); i++)
        if (ops[i].type == FSOP_TEXT)
            uppercase(ops[i].text);
}

void formatted_string::capitalise()
{
    for (unsigned int i = 0; i < ops.size(); i++)
        if (ops[i].type == FSOP_TEXT && !ops[i].text.empty())
        {
            ops[i].text = uppercase_first(ops[i].text);
            break;
        }
}

void formatted_string::filter_lang()
{
    for (unsigned int i = 0; i < ops.size(); i++)
        if (ops[i].type == FSOP_TEXT)
            ::filter_lang(ops[i].text);
}

int count_linebreaks(const formatted_string& fs)
{
    string::size_type where = 0;
    const string s = fs;
    int count = 0;
    while (1)
    {
        where = s.find("\n", where);
        if (where == string::npos)
            break;
        else
        {
            ++count;
            ++where;
        }
    }
    return count;
}

static int _tagged_string_printable_length(const string& s)
{
    int len = 0;
    bool in_tag = false;
    int last_taglen = 0;
    for (string::const_iterator ci = s.begin(); ci != s.end(); ++ci)
    {
        if (in_tag)
        {
            if (*ci == '<' && last_taglen == 1) // "<<" sequence
            {
                in_tag = false;  // this is an escape for '<'
                ++len;           // len wasn't incremented before
            }
            else if (*ci == '>') // tag close, still nothing printed
                in_tag = false;
            else                 // tag continues
                ++last_taglen;
        }
        else if (*ci == '<')     // tag starts
        {
            in_tag = true;
            last_taglen = 1;
        }
        else                     // normal, printable character
            ++len;
    }
    return len;
}

// Count the length of the tags in the string.
int tagged_string_tag_length(const string& s)
{
    return s.size() - _tagged_string_printable_length(s);
}

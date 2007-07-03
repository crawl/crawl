#include "AppHdr.h"

#include "initfile.h"
#include "format.h"
#include "view.h"

formatted_string::formatted_string(int init_colour)
    : ops()
{
    if (init_colour)
        textcolor(init_colour);
}

formatted_string::formatted_string(const std::string &s, int init_colour)
    : ops()
{
    if (init_colour)
        textcolor(init_colour);
    cprintf(s);
}

int formatted_string::get_colour(const std::string &tag)
{
    if (tag == "h")
        return (YELLOW);

    if (tag == "w")
        return (WHITE);

    const int colour = str_to_colour(tag);
    return (colour != -1? colour : LIGHTGREY);
}

formatted_string formatted_string::parse_block(
        const std::string &s,
        bool  eol_ends_format,
        bool (*process)(const std::string &tag))
{
    std::vector<std::string> lines = split_string("\n", s, false, true);

    formatted_string fs;
    for (int i = 0, size = lines.size(); i < size; ++i)
    {
        if (i)
        {
            // Artificial newline - some terms erase to eol when printing a
            // newline.
            fs.gotoxy(1, -1); // CR
            fs.movexy(0, 1);  // LF
        }
        fs += parse_string(lines[i], eol_ends_format, process);
    }

    return (fs);
}

formatted_string formatted_string::parse_string(
        const std::string &s,
        bool  eol_ends_format,
        bool (*process)(const std::string &tag))
{
    // FIXME This is a lame mess, just good enough for the task on hand
    // (keyboard help).
    formatted_string fs;
    std::string::size_type tag    = std::string::npos;
    std::string::size_type length = s.length();

    std::string currs;
    int curr_colour = LIGHTGREY;
    bool masked = false;
    
    for (tag = 0; tag < length; ++tag)
    {
        bool invert_colour = false;
        std::string::size_type endpos = std::string::npos;

        if (s[tag] != '<' || tag >= length - 2)
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
        if (endpos == std::string::npos)
        {
            if (!masked)
                currs += s[tag];
            continue;
        }

        std::string tagtext = s.substr(tag + 1, endpos - tag - 1);
        if (tagtext.empty() || tagtext == "/")
        {
            if (!masked)
                currs += s[tag];
            continue;
        }

        if (tagtext[0] == '/')
        {
            invert_colour = true;
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

        const int new_colour = invert_colour? LIGHTGREY : get_colour(tagtext);
        if (!currs.empty())
        {
            fs.cprintf(currs);
            currs.clear();
        }
        fs.textcolor( curr_colour = new_colour );
        tag += tagtext.length() + 1;
    }
    if (currs.length())
        fs.cprintf(currs);

    if (eol_ends_format && curr_colour != LIGHTGREY)
        fs.textcolor(LIGHTGREY);

    return (fs);
}

formatted_string::operator std::string() const
{
    std::string s;
    for (unsigned i = 0, size = ops.size(); i < size; ++i)
    {
        if (ops[i] == FSOP_TEXT)
            s += ops[i].text;
    }
    return (s);
}

const formatted_string &
formatted_string::operator += (const formatted_string &other)
{
    ops.insert(ops.end(), other.ops.begin(), other.ops.end());
    return (*this);
}

std::string::size_type formatted_string::length() const
{
    // Just add up the individual string lengths.
    std::string::size_type len = 0;
    for (unsigned i = 0, size = ops.size(); i < size; ++i)
        if (ops[i] == FSOP_TEXT)
            len += ops[i].text.length();
    return (len);
}

inline void cap(int &i, int max)
{
    if (i < 0 && -i <= max)
        i += max;
    if (i >= max)
        i = max - 1;
    if (i < 0)
        i = 0;
}

std::string formatted_string::tostring(int s, int e) const
{
    std::string st;
    
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

void formatted_string::gotoxy(int x, int y)
{
    ops.push_back( fs_op(x, y) );
}

void formatted_string::movexy(int x, int y)
{
    ops.push_back( fs_op(x, y, true) );
}

int formatted_string::find_last_colour() const
{
    if (!ops.empty())
    {
        for (int i = ops.size() - 1; i >= 0; --i)
        {
            if (ops[i].type == FSOP_COLOUR)
                return (ops[i].x);
        }
    }
    return (LIGHTGREY);
}

void formatted_string::add_glyph(const item_def *item)
{
    const int last_col = find_last_colour();
    unsigned ch;
    unsigned short col;
    get_item_glyph(item, &ch, &col);
    this->textcolor(col);
    this->cprintf("%c", ch);
    this->textcolor(last_col);
}

void formatted_string::add_glyph(const monsters *mons)
{
    const int last_col = find_last_colour();
    unsigned ch;
    unsigned short col;
    get_mons_glyph(mons, &ch, &col);
    this->textcolor(col);
    this->cprintf("%c", ch);
    this->textcolor(last_col);
}

void formatted_string::textcolor(int color)
{
    if (!ops.empty() && ops[ ops.size() - 1 ].type == FSOP_COLOUR)
        ops.erase( ops.end() - 1 );
    
    ops.push_back(color);
}

void formatted_string::clear()
{
    ops.clear();
}

void formatted_string::cprintf(const char *s, ...)
{
    char buf[1000];
    va_list args;
    va_start(args, s);
    vsnprintf(buf, sizeof buf, s, args);
    va_end(args);

    cprintf(std::string(buf));
}

void formatted_string::cprintf(const std::string &s)
{
    ops.push_back(s);
}

void formatted_string::fs_op::display() const
{
    switch (type)
    {
    case FSOP_CURSOR:
    {
        int cx = x, cy = y;
        if (relative)
        {
            cx += wherex();
            cy += wherey();
        }
        else
        {
            if (cx == -1)
                cx = wherex();
            if (cy == -1)
                cy = wherey();
        }
        ::gotoxy(cx, cy);
        break;
    }
    case FSOP_COLOUR:
        ::textattr(x);
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

int count_linebreaks(const formatted_string& fs)
{
    std::string::size_type where = 0;
    const std::string s = fs;
    int count = 0;
    while ( 1 )
    {
        where = s.find(EOL, where);
        if ( where == std::string::npos )
            break;
        else
        {
            ++count;
            ++where;
        }
    }
    return count;
}

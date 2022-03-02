#include "AppHdr.h"

#include "format.h"

#include <climits>

#include "cio.h"
#include "colour.h"
#include "lang-fake.h"
#include "libutil.h"
#include "stringutil.h"
#include "unicode.h"
#include "viewchar.h"

formatted_string::formatted_string(int init_colour)
    : ops()
{
    if (init_colour)
        textcolour(init_colour);
}

formatted_string::formatted_string(const string &s, int init_colour)
    : ops()
{
    if (init_colour)
        textcolour(init_colour);
    cprintf(s);
}

/**
 * For a given tag, return the corresponding colour.
 *
 * @param tag       The tag: e.g. "red", "lightblue", "h", "w".
 * @return          The corresponding colour (e.g. RED), or LIGHTGREY.
 */
int formatted_string::get_colour(const string &tag)
{
    if (starts_with(tag, "bg:"))
        return get_colour(tag.substr(3));

    if (tag == "h")
        return YELLOW;

    if (tag == "w")
        return WHITE;

    return str_to_colour(tag);
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

/**
 * Take a string and turn it into a formatted_string.
 *
 * @param s             The input string: e.g. "<red>foo</red>".
 * @param main_colour   The initial & default text colour.
 * @return          A formatted string corresponding to the input.
 */
formatted_string formatted_string::parse_string(const string &s,
                                                int main_colour)
{
    // main_colour will usually be LIGHTGREY (default).
    vector<int> colour_stack(1, main_colour);
    // background will not change anything by default
    vector<int> bg_stack(1, BLACK);

    formatted_string fs;

    parse_string1(s, fs, colour_stack, bg_stack);
    if (colour_stack.back() != colour_stack.front())
        fs.textcolour(colour_stack.front()); // XXX: this does nothing
    // if the bg colours are unbalanced, reset to black
    if (bg_stack.back() != bg_stack.front())
        fs.textbackground(bg_stack.back());
    return fs;
}

// Parses a formatted string in much the same way as parse_string, but
// handles \n by creating a new formatted_string.
void formatted_string::parse_string_to_multiple(const string &s,
                                                vector<formatted_string> &out,
                                                int wrap_col)
{
    vector<int> colour_stack(1, LIGHTGREY);
    vector<int> bg_stack(1, BLACK);

    vector<string> lines = split_string("\n", s, false, true);
    if (wrap_col > 0)
    {
        vector<string> pre_split = move(lines);
        for (string &line : pre_split)
        {
            do
            {
                lines.push_back(wordwrap_line(line, wrap_col, true, true));
            }
            while (!line.empty());
        }
    }

    for (const string &line : lines)
    {
        out.emplace_back();
        formatted_string& fs = out.back();
        // apply initial colors or the colors leftover on the stack from
        // previous lines. Only apply background in case there was an explicit
        // bg tag from previous lines.
        fs.textcolour(colour_stack.back());
        if (bg_stack.size() > 1)
            fs.textbackground(bg_stack.back());
        parse_string1(line, fs, colour_stack, bg_stack);
        // if the colour strings are unbalanced at this point, reset the
        // colors at the end of the line, and then re-apply them to the next
        // line (if there is one) on the next cycle of this loop
        if (colour_stack.back() != colour_stack.front())
            fs.textcolour(colour_stack.front()); // XXX: this does nothing
        if (bg_stack.back() != bg_stack.front())
            fs.textbackground(bg_stack.front());
    }
}

// Helper for the other parse_ methods.
void formatted_string::parse_string1(const string &s, formatted_string &fs,
                                     vector<int> &colour_stack,
                                     vector<int> &bg_stack)
{
    // FIXME: This is a lame mess, just good enough for the task on hand
    // (keyboard help).
    string::size_type tag    = string::npos;
    string::size_type length = s.length();

    string currs;

    for (tag = 0; tag < length; ++tag)
    {
        bool revert_colour = false;
        bool bg = false;
        string::size_type endpos = string::npos;

        // Break string up if it gets too big.
        // XX why is 999 "too big"???
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
            currs += s[tag];
            continue;
        }

        // Is this a << escape?
        if (s[tag + 1] == '<')
        {
            currs += s[tag];
            tag++;
            continue;
        }

        // trying to parse a color tag below here
        endpos = s.find('>', tag + 1);
        // No closing >?
        if (endpos == string::npos)
        {
            currs += s[tag];
            continue;
        }

        string tagtext = s.substr(tag + 1, endpos - tag - 1);

        // invalid tag: `<>` or `</>`. Just ignore.
        if (tagtext.empty() || tagtext == "/")
        {
            currs += s[tag];
            continue;
        }

        if (tagtext[0] == '/')
        {
            revert_colour = true;
            tagtext = tagtext.substr(1);
            tag++;
        }

        if (starts_with(tagtext, "bg:"))
            bg = true;

        if (!currs.empty())
        {
            fs.cprintf(currs);
            currs.clear();
        }

        auto &cur_stack = bg ? bg_stack : colour_stack;
        const int colour = get_colour(tagtext);

        if (revert_colour)
        {
            if (cur_stack.size() > 1 && colour == cur_stack.back())
                cur_stack.pop_back();
            else
            {
                // If this was the only tag, or the colour didn't match
                // the one we are popping, display the tag as a warning.
                // applies to both background and foreground
                fs.textcolour(LIGHTRED);
                fs.cprintf("</%s>", tagtext.c_str());
            }
        }
        else
        {
            if (colour == -1)
            {
                fs.textcolour(LIGHTRED);
                fs.cprintf("<%s>", tagtext.c_str());
            }
            else
                cur_stack.push_back(colour);
        }

        // fs.cprintf("%d%d", colour_stack.size(), colour_stack.back());
        if (bg)
            fs.textbackground(bg_stack.back());
        else
            fs.textcolour(colour_stack.back());

        tag += tagtext.length() + 1;
    }
    if (currs.length())
        fs.cprintf(currs);
}

/// Return a plaintext version of this string, sans tags, colours, etc.
formatted_string::operator string() const
{
    string s;
    for (const fs_op &op : ops)
        if (op.type == FSOP_TEXT)
            s += op.text;

    return s;
}

bool formatted_string::operator < (const formatted_string &other) const
{
    return string(*this) < string(other);
}

bool formatted_string::operator == (const formatted_string &other) const
{
    // May produce false negative in some cases, e.g. duplicated colour ops
    return ops == other.ops;
}

const formatted_string &
formatted_string::operator += (const formatted_string &other)
{
    ops.insert(ops.end(), other.ops.begin(), other.ops.end());
    return *this;
}

const formatted_string &
formatted_string::operator += (const string& other)
{
    ops.emplace_back(other);
    return *this;
}

int formatted_string::width() const
{
    // Just add up the individual string lengths.
    int len = 0;
    for (const fs_op &op : ops)
        if (op.type == FSOP_TEXT)
            len += strwidth(op.text);
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
        if (ops[i].type != FSOP_TEXT)
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
        if (ops[i].type == FSOP_TEXT)
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
        switch (ops[i].type)
        {
        case FSOP_TEXT:
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
            break;
        }
        // apparently don't write any closing tags (which the parser can handle)
        case FSOP_COLOUR:
            st += "<";
            st += colour_to_str(ops[i].colour);
            st += ">";
            break;
        case FSOP_BG:
            st += "<bg:";
            st += colour_to_str(ops[i].colour);
            st += ">";
            break;
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
                return ops[i].colour;
    }
    return LIGHTGREY;
}

formatted_string formatted_string::chop(int length, bool pad) const
{
    formatted_string result;
    for (const fs_op& op : ops)
    {
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
    if (pad && length > 0)
        result += string(length, ' ');

    return result;
}

formatted_string formatted_string::chop_bytes(int length) const
{
    return substr_bytes(0, length);
}

formatted_string formatted_string::substr_bytes(int pos, int length) const
{
    formatted_string result;
    fs_op initial(LIGHTGREY);
    for (const fs_op& op : ops)
    {
        if (op.type == FSOP_TEXT)
        {
            int n = op.text.size();
            if (pos >= n)
            {
                pos -= n;
                continue;
            }
            if (result.empty())
                result.ops.push_back(initial);
            result.ops.push_back(fs_op(op.text.substr(pos, length)));
            string& new_string = result.ops[result.ops.size()-1].text;
            pos = 0;
            length -= new_string.size();
            if (length <= 0)
                break;
        }
        else if (pos == 0)
            result.ops.push_back(op);
        else
            initial = op;
    }
    return result;
}

formatted_string formatted_string::trim() const
{
    return parse_string(trimmed_string(to_colour_string()));
}

void formatted_string::del_char()
{
    for (auto i = ops.begin(); i != ops.end(); ++i)
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
    // cglyph_t does not affect background colour
    const int last_col = find_last_colour();
    textcolour(g.col);
    cprintf("%s", stringize_glyph(g.ch).c_str());
    textcolour(last_col);
}

void formatted_string::textcolour(int colour)
{
    if (!ops.empty() && ops.back().type == FSOP_COLOUR)
        ops.pop_back();

    ops.emplace_back(colour);
}

void formatted_string::textbackground(int colour)
{
    if (!ops.empty() && ops.back().type == FSOP_BG)
        ops.pop_back();

    ops.emplace_back(colour, false);
}

void formatted_string::clear()
{
    ops.clear();
}

bool formatted_string::empty() const
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
    // see also FTFontWrapper::store
    switch (type)
    {
    case FSOP_COLOUR:
#ifndef USE_TILE_LOCAL
        if (colour < NUM_TERM_COLOURS)
#endif
            ::textcolour(colour);
        break;
    case FSOP_BG:
#ifndef USE_TILE_LOCAL
        if (colour < NUM_TERM_COLOURS)
#endif
            ::textbackground(colour);
        break;
    case FSOP_TEXT:
        wrapcprintf("%s", text.c_str());
        break;
    }
}

void formatted_string::swap(formatted_string& other)
{
    ops.swap(other.ops);
}

void formatted_string::all_caps()
{
    for (fs_op &op : ops)
        if (op.type == FSOP_TEXT)
            uppercase(op.text);
}

void formatted_string::capitalise()
{
    for (fs_op &op : ops)
        if (op.type == FSOP_TEXT && !op.text.empty())
        {
            op.text = uppercase_first(op.text);
            break;
        }
}

void formatted_string::filter_lang()
{
    for (fs_op &op : ops)
        if (op.type == FSOP_TEXT)
            ::filter_lang(op.text);
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

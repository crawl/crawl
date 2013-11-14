#include "AppHdr.h"

#include "tagstring.h"

#include "colour.h"

string colour_string(string in, int col)
{
    if (in.empty())
        return in;
    const string cols = colour_to_str(col);
    return "<" + cols + ">" + in + "</" + cols + ">";
}

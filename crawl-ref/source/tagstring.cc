#include "AppHdr.h"

#include "tagstring.h"

#include "colour.h"

std::string colour_string(std::string in, int col)
{
    if (in.empty())
        return (in);
    const std::string cols = colour_to_str(col);
    return ("<" + cols + ">" + in + "</" + cols + ">");
}

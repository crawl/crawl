
#include <string>

#include "maybe-bool.h"

bool tobool(maybe_bool mb, bool def)
{
    return mb.to_bool(def);
}

maybe_bool frombool(bool b)
{
    return maybe_bool(b);
}

const std::string maybe_to_string(const maybe_bool mb)
{
    return mb.to_string();
}

// probably would be safe to move to header and inline?
const std::string maybe_bool::to_string() const
{
    switch (value)
    {
    case mbool_t::t:     return "true";
    case mbool_t::f:     return "false";
    default: // or die?
    case mbool_t::maybe: return "maybe";
    }
}

// I think in c++17 on, these could be `inline` in the header...
const maybe_bool maybe_bool::maybe(maybe_bool::mbool_t::maybe);
const maybe_bool maybe_bool::t(true);
const maybe_bool maybe_bool::f(false);

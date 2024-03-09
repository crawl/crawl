
#include "AppHdr.h"

#include <string>

#include "debug.h"
#include "maybe-bool.h"

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

void maybe_bool::test_cases()
{
    maybe_bool n(false);
    maybe_bool y(true);
    const maybe_bool m(maybe_bool::maybe);

    ASSERT(maybe_bool::f == n);
    ASSERT(maybe_bool::f != y);
    ASSERT(maybe_bool::t == y);
    ASSERT(maybe_bool::t != n);
    ASSERT(maybe_bool::f != m);
    ASSERT(maybe_bool::t != m);
    ASSERT(maybe_bool::maybe == m);
    ASSERT(maybe_bool::maybe != y);
    ASSERT(maybe_bool::maybe != n);

    ASSERT(maybe_bool(false) == false);
    ASSERT(maybe_bool(false) != true);
    ASSERT(maybe_bool(true) != false);
    ASSERT(maybe_bool(true) == true);
    ASSERT(maybe_bool::maybe != true);
    ASSERT(maybe_bool::maybe != false);

    ASSERT(static_cast<bool>(maybe_bool::t));
    ASSERT(!static_cast<bool>(maybe_bool::maybe));
    ASSERT(!static_cast<bool>(maybe_bool::f));
    ASSERT(!static_cast<bool>(!maybe_bool::t));
    ASSERT(!static_cast<bool>(!maybe_bool::maybe));
    ASSERT(static_cast<bool>(!maybe_bool::f));

    ASSERT((!y) == n);
    ASSERT((!n) == y);
    ASSERT((!m) == m);

    ASSERT((y && y) == y);
    ASSERT((y && m) == m);
    ASSERT((y && n) == n);
    ASSERT((m && y) == m);
    ASSERT((m && m) == m);
    ASSERT((m && n) == n);
    ASSERT((n && y) == n);
    ASSERT((n && m) == n);
    ASSERT((n && n) == n);

    ASSERT((y || y) == y);
    ASSERT((y || m) == y);
    ASSERT((y || n) == y);
    ASSERT((m || y) == y);
    ASSERT((m || m) == m);
    ASSERT((m || n) == m);
    ASSERT((n || y) == y);
    ASSERT((n || m) == m);
    ASSERT((n || n) == n);
}

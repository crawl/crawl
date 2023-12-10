#pragma once

#include <string>

// inspired by boost's logic::tribool, but with a much simpler implementation
//    (https://github.com/boostorg/logic/blob/develop/include/boost/logic/tribool.hpp)
// This essentially is a Kleene strong three-valued boolean, that requires an
// explicit cast to use as a bool.
//
// Usage notes:
// Construction and casts:
//     maybe_bool(true) == maybe_bool::t
//     maybe_bool(false) == maybe_bool::f
//     bool(maybe_bool::t) == true
//     bool(maybe_bool::f) == false
//     bool(maybe_bool::maybe) == false
//
// Operators (these are the Kleene strong operators):
//     with t as maybe_bool::t, m as maybe_bool::maybe, and f as maybe_bool::f:
//
//     !: !t == f, !f == t, !m == m
//
//     &&:    f m t     ||:    f m t
//         --------         --------
//         f |f f f         f |f m t
//         m |f m m         m |m m t
//         t |f m t         t |t t t
//
//     (see note below re mixing maybe_bool with bool)
//
// Writing `true` and `false` for type maybe_bool is safe; comparison against
// these values will do exact comparison. (So no element in `bool` is equal to
// `maybe_bool::maybe`.)
//
// The c++11 explicit bool cast rules are somewhat involved. First, despite
// the `explicit`, there are all sorts of scenarios where this will implicitly
// cast to bool, primarily in control flow expressions that are looking for
// a bool. Because of the existence of && and || operators, it will not cast
// to bool automatically in expressions that mix maybe_bool and bool. It will
// not cast to bool automatically an a return statement for a function with
// return type bool. A condition with just negation is ok, but negation plus
// a binary operator + bool will require a cast.
//
// A specific pitfall: if you need to explicitly cast to bool in combination
// with negation, you usually want `bool(!p)` (which is true just in
// case p is exactly false) rather than `!bool(p)` (which is true just in case
// p is either false or maybe). If you do want the latter semantics, it's
// probably clearer to write something like `p != true`.
//
// conditions cheat sheet; the most idiomatic way to handle common conditions
// (arguably):
// p is exactly true:   `if (p) ...`
// p is exactly false:  `if (!p) ...`
// p is true or maybe:  `if (p != false) ...`   (or `if (p.to_bool(true)) ...`)
// p is false or maybe: `if (p != true) ...`    (or, possibly, `if (!p.to_bool(false)) ...`)
// p is true or false:  `if (p.is_bool()) ...`
// p is maybe:          `if (p == maybe_bool::maybe) ...`  (or, possibly, `if (!p.is_bool())) ...`

class maybe_bool
{
protected:
    enum class mbool_t { t, f, maybe } value;
    constexpr maybe_bool(const mbool_t val)
        : value(val)
    { }

public:
    constexpr maybe_bool()
        : maybe_bool(mbool_t::f)
    { }

    constexpr maybe_bool(bool b)
        : maybe_bool(b ? mbool_t::t : mbool_t::f)
    { }

    // different approach than tribool: we define a static member variable,
    // so that you use a scoped expression (maybe_bool::maybe) to refer to the
    // third value.
    const static maybe_bool maybe;
    const static maybe_bool t;
    const static maybe_bool f;

    inline bool is_bool() const
    {
        return value != mbool_t::maybe;
    }

    const std::string to_string() const;

    // note! explicit bool operator gets called implicitly in a variety of
    // circumstances (aka implicit contextual conversion).
    // default explicit cast rules map maybe to false (which is a change
    // from the old enum implementation)
    inline bool to_bool(const bool def=true) const
    {
        switch (value)
        {
        case mbool_t::t:     return true;
        case mbool_t::f:     return false;
        default: // avoid warnings
        case mbool_t::maybe: return def;
        }
    }
    inline explicit operator bool() const { return to_bool(false); }

    // implicit construction *from* bool is allowed, getting the full range
    // of maybe_bool to bool comparisons as well
    friend inline bool operator== (const maybe_bool &b1, const maybe_bool &b2);
    friend inline bool operator!= (const maybe_bool &b1, const maybe_bool &b2);
    // TODO: maybe > etc?

    friend inline maybe_bool operator&& (const maybe_bool &b1, const maybe_bool &b2);
    friend inline maybe_bool operator|| (const maybe_bool &b1, const maybe_bool &b2);
    friend inline maybe_bool operator! (const maybe_bool &b);

    static void test_cases();
};

inline bool operator== (const maybe_bool &b1, const maybe_bool &b2)
{
    return b1.value == b2.value;
}

inline bool operator!= (const maybe_bool &b1, const maybe_bool &b2)
{
    return b1.value != b2.value;
}

// Kleene strong and, or, not:
inline maybe_bool operator&& (const maybe_bool &b1, const maybe_bool &b2)
{
    return b1 == maybe_bool::f || b2 == maybe_bool::f         ? maybe_bool::f
         : b1 == maybe_bool::maybe || b2 == maybe_bool::maybe ? maybe_bool::maybe
                                                              : maybe_bool::t;
}

inline maybe_bool operator|| (const maybe_bool &b1, const maybe_bool &b2)
{
    return b1 == maybe_bool::t || b2 == maybe_bool::t         ? maybe_bool::t
         : b1 == maybe_bool::maybe || b2 == maybe_bool::maybe ? maybe_bool::maybe
                                                              : maybe_bool::f;
}

inline maybe_bool operator! (const maybe_bool &b)
{
    return b == maybe_bool::maybe ? b
                                  : !(static_cast<bool>(b));
}

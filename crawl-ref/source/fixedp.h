#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <ostream>
#include <string>
#include <type_traits>

/**
 * Header-only decimal fixed point library for DCSS.
 * Author: Kyle Rawlins
 *
 * This class is a minimalist wrapper around the way that DCSS already
 * approaches fixed point calculations, namely by using an integer value
 * multiplied by some scale parameter. It's not a general-purpose
 * fixed-point library; see e.g. https://github.com/johnmcfarlane/fixed_point
 * and https://github.com/johnmcfarlane/cnl. Unfortunately the fixed point
 * ecosystem is in something of a state of flux, and I decided that the
 * existing libraries were just too heavy/complicated to use for our purposes.
 * This differs from the code it is replacing in one qualitative way:
 * it allows rounding, rather than truncation behavior in division and
 * multiplication. The fact that dcss has traditionally used decimal scales
 * also impacts this class: a more general fixed point library would use a
 * binary point. This class loses out on the various optimizations that you can
 * do easily with binary fixed point arithmetic (efficient rounding, shift for
 * divide, etc.).
 *
 * Down the line, perhaps that CNL class (or something else) will develop into
 * a C++ standard and this can be replaced.
 *
 * This implements most operators you would want, as well as many cmath-style
 * functions. Casts out of fixedp are all explicit, but this allows implicit
 * casts from int, long, and float.
 *
 * operators: +=, -=, *=, /=, +, -, *, /, <, <=, >, >=, ==, !=, <<, unary -,
 * ++ (prefix, postifx), -- (prefix, postfix).
 *
 * casts: explicit casts to int, long, bool, float. int and long use trancating
 * behavior.
 *
 * constructors: int, long, unsigned int, unsigned long, float, double. See
 * also the static factory functions from_scaled, from_basetype, and
 * from_fixedp (which allows converting between precisions).
 *
 * cmath-style functions: trunc, round, ceil, floor, abs, max, min
 *
 * By default, multiplication/division use rounding rather than truncation. You
 * can toggle this behavior with `set_rounding`.
 *
 * Warning: this has no overflow protection/detection, so if you use a big Scale
 * parameter, or multiply/divide by large numbers, be careful of how much space
 * you have. fixedp should work with any integral type so in a pinch you can use
 * bigger types.
 *
 * @tparam BaseType the base type to use for storing numbers. Must be integral.
 * @tparam Scale the scale value to use: a constant factor that determines where
 * the decimal is in the underlying representation.
 *
 * Examples:
 *
 * > auto AC = 4 + fixedp<>(experience_level) / 3
 * Safely (to .01 precision) compute a fractional AC value, for nagas. This
 * makes use of implicit constructors forced by arithmetic with a fixedp.
 *
 * > return 2 + fixedp<>(experience_level) * 2 / 5 + (max(0, fixedp<>(experience_level) - 7) * 2 / 5);
 * A more elaborate example with implicit constructors, in this case for
 * calculating gargoyle AC at a given XL. Makes use of max.
 *
 * See fixedp::test_cases for further examples.
 *
 * Using a Scale that is a power of 2 is the most efficient use of precision,
 * but using a Scale that is a power of 10 is much more transparent. This class
 * contains no particular optimization for the power of 2 case, and is mainly
 * intended for use with a decimal point.
 */
template <typename BaseType=int, int Scale=100>
class fixedp
{
    static_assert(std::is_integral<BaseType>(),
                                    "fixedp requires an integral base type.");
    // it's *much* more convenient to use `int` and have this assert, than to
    // use `unsigned`.
    static_assert(Scale > 0, "fixedp requires a positive scale.");

    // the following is very weak...
    // TODO: could instead require at least 1 bits of integral space?
    static_assert(Scale <= std::numeric_limits<BaseType>::max(),
                "fixedp requires that Scale not exceed the limit of BaseType!");

    // the next two functions use type traits/SFINAE to implement a version of
    // abs that is safe for unsigned types; it will return the unsigned as-is
    // and not trigger any warnings or errors.  (Using regular abs on an
    // unsigned leads to both warnings and errors in a template context.)
    //
    // This is used to calculate the fractional part safely for all types (and
    // not used by the actual abs, which intentionally has undefined behavior
    // when BaseType is unsigned).

    template<typename T=BaseType> T _safe_abs(T n,
            typename std::enable_if<std::is_signed<T>::value, T>::type=0) const
    {
        using std::abs;
        return static_cast<T>(abs(n));
    }

    template<typename T=BaseType> T constexpr _safe_abs(T n,
            typename std::enable_if<!std::is_signed<T>::value, T>::type=0) const
    {
        return n;
    }

    BaseType content;
    static bool rounding;
public:
    // default constructor for sanity's sake. This is different than builtin
    // numeric types, which have undefined behavior if not initialized!
    fixedp()
        : content(0)
    {
    }

    // copy constructor
    fixedp(const fixedp<BaseType, Scale> &rhs)
        : content(rhs.content)
    {
    }

    // TODO: overflow check in constructors? (|amethyst)
    constexpr fixedp(int n) : content(static_cast<BaseType>(n) *
                                                static_cast<BaseType>(Scale))
    {
    }
    constexpr fixedp(long n) : content(static_cast<BaseType>(n) *
                                                static_cast<BaseType>(Scale))
    {
    }
    constexpr fixedp(unsigned int n) : content(static_cast<BaseType>(n) *
                                                static_cast<BaseType>(Scale))
    {
    }
    constexpr fixedp(unsigned long n) : content(static_cast<BaseType>(n) *
                                                static_cast<BaseType>(Scale))
    {
    }

    // is rounding here really a good idea?
    // TODO: per amalloy, should these really be implicit? It's awfully
    // convenient.
    constexpr fixedp(float n) : content(std::round(n *
                                                static_cast<float>(Scale)))
    {
    }

    constexpr fixedp(double n) : content(std::round(n *
                                                static_cast<double>(Scale)))
    {
    }

    /**
     * Construct a fixedp from an integer plus a fraction of Scale. This does
     * no sanity checking on frac, so use it carefully. In particular,
     * int_part and frac should have the same sign, and frac should be <
     * int_part.
     */
    constexpr fixedp(int int_part, int frac)
        : content(static_cast<BaseType>(int_part) *
                    static_cast<BaseType>(Scale) + static_cast<BaseType>(frac))
    {
    }

    template <int OtherScale> explicit fixedp (fixedp<BaseType, OtherScale> n)
        : content(from_fixedp<OtherScale>(n).to_scaled())
    {
    }


    /**
     * Factory function, not a constructor. We don't want this to participate
     * in arithmetic.
     *
     * @param n a value that has already been scaled by Scale to initialize the
     * fixedp with.
     */
    static fixedp from_scaled(const BaseType n)
    {
        // TODO: this could be constexpr on c++14
        fixedp ret(0);
        ret.content = n;
        return ret;
    }

    static fixedp from_basetype(const BaseType i, const BaseType frac)
    {
        return from_scaled(i * static_cast<BaseType>(Scale) + frac);
    }


    /**
     * Convert from a fixedp with a different Scale.
     *
     * This uses fixedp division when lowering precision, and so will inherit
     * its rounding behavior from the containing class.
     *
     * @tparam OtherScale the scale of the other fixedp.
     * @param n the fixedp to convert from.
     */
    template <int OtherScale>
        static fixedp from_fixedp(fixedp<BaseType, OtherScale> n)
    {
        // TODO: conversion across BaseTypes?
        return (fixedp<BaseType, Scale>::from_basetype(n.frac_part(true), 0)
                            / OtherScale)
                + fixedp<BaseType, Scale>::from_basetype(n.integral_part(), 0);
    }

    /**
     * Set the rounding behavior of multiplication/division on a per-class
     * basis.
     */
    static void set_rounding(bool r)
    {
        rounding = r;
    }

    BaseType constexpr to_scaled() const
    {
        return content;
    }

    /**
     * Give the fractional part of the fixedp.
     *
     * @param signed whether to provide a signed or absolute version. This is
     * safe with unsigned BaseTypes.
     */
    BaseType frac_part(bool calc_signed = false) const
    {
        // This needs to use a somewhat more complicated version of abs in
        // order to work with unsigned types.
        return (!calc_signed || content >= 0)
            ? _safe_abs<BaseType>(content) % static_cast<BaseType>(Scale)
            : -(_safe_abs<BaseType>(content) % static_cast<BaseType>(Scale));
    }

    /**
     * Give the integral part (de-scaled) of the fixedp. Preserves sign.
     */
    BaseType constexpr integral_part() const
    {
        return content / static_cast<BaseType>(Scale);
    }

    fixedp trunc() const
    {
        return from_scaled(content - frac_part(true));
    }

    bool rounds_nearby() const
    {
        // TODO: better handling of non-even Scale?
        return frac_part() >= Scale / 2;
    }

    fixedp round() const
    {
        // this is a bit ugly...
        return rounds_nearby() ? (content >= 0 ? ++trunc()
                                               : --trunc())
                               : trunc();
    }

    // provide some cmath-style functions
    friend inline fixedp round(const fixedp &n)
    {
        return n.round();
    }
    friend inline fixedp trunc(const fixedp &n)
    {
        return n.trunc();
    }
    friend inline fixedp ceil(const fixedp &n)
    {
        return n.frac_part() == 0 ? n :
                (n.content >= 0 ? ++n.trunc() : n.trunc());
    }
    friend inline fixedp floor(const fixedp &n)
    {
        return n.frac_part() == 0 ? n :
                (n.content >= 0 ? n.trunc() : --n.trunc());
    }
    friend inline fixedp abs(const fixedp &n)
    {
        static_assert(std::is_signed<BaseType>(),
            "Using abs() with fixedp requires a signed BaseType.");

        // Even without the static_assert, this won't work with an unsigned
        // BaseType because abs is ambiguous without an explicit cast.
        using std::abs;
        return from_scaled(abs(n.content));
    }
    friend inline fixedp max(const fixedp &l, const fixedp &r)
    {
        return (l.content > r.content) ? l : r;
    }
    friend inline fixedp min(const fixedp &l, const fixedp &r)
    {
        return (l.content < r.content) ? l : r;
    }

    // assignment -- uses copy & swap paradigm
    void swap(fixedp rhs)
    {
        using std::swap;
        swap(content, rhs.content);
    }

    fixedp& operator=(fixedp rhs)
    {
        swap(rhs);
        return *this;
    }

    // conversion, explicit only
    explicit constexpr operator int() const
    {
        // truncates, following standard behavior of cast to integral types
        return static_cast<int>(integral_part());
    }
    explicit constexpr operator bool() const
    {
        return static_cast<bool>(content);
    }
    explicit constexpr operator long() const
    {
        // truncates, following standard behavior of cast to integral types
        return static_cast<long>(integral_part());
    }
    explicit operator float() const
    {
        return static_cast<float>(integral_part()) +
            static_cast<float>(frac_part(true) / static_cast<float>(Scale));
    }
    explicit operator double() const
    {
        return static_cast<double>(integral_part()) +
            static_cast<double>(frac_part(true) / static_cast<double>(Scale));
    }

    // string and stream output

    // to use fixedp with printf, your best options are things like:
    // > printf("%g", (float) n);
    // > printf("%.2f", (float) n);
    // using printf with the casts to str is not really recommended.

    std::string str() const
    {
        // TODO: chop trailing zeros / adjust precision?
        // would prefer %g behavior here. stream works better...
        return std::to_string((float) *this);
    }
    explicit operator std::string() const
    {
        return str();
    }
    inline friend std::string to_string(const fixedp& n)
    {
        return n.str();
    }
    inline friend std::ostream &operator<< (std::ostream &os, fixedp const &m)
    {
        // use the cast to float so that float formatting options can
        // be applied.
        os << (float) m;
        return os;
    }

    // comparison with other fixedps at the same template parameters
    // to cast acrtoss different fixedps, you'll need to explicitly convert
    // to a common type

    // because the constructors from numeric types are implicit, these are also
    // used for comparison with regular numerics.

    inline friend bool operator==(const fixedp& lhs, const fixedp& rhs)
    {
        return lhs.content == rhs.content;
    }
    inline friend bool operator!=(const fixedp& lhs, const fixedp& rhs)
    {
        return !operator==(lhs, rhs);
    }

    inline friend bool operator< (const fixedp& lhs, const fixedp& rhs)
    {
        return lhs.content < rhs.content;
    }
    inline friend bool operator> (const fixedp& lhs, const fixedp& rhs)
    {
        return  operator< (rhs, lhs);
    }
    inline friend bool operator<=(const fixedp& lhs, const fixedp& rhs)
    {
        return !operator> (lhs, rhs);
    }
    inline friend bool operator>=(const fixedp& lhs, const fixedp& rhs)
    {
        return !operator< (lhs,rhs);
    }

    // arithmetic with other fixedps

    // because the constructors from numeric types are implicit, these are also
    // used for arithmetic with regular numerics.

    fixedp operator-() const
    {
        static_assert(std::is_signed<BaseType>(),
            "Using unary - with fixedp requires a signed BaseType.");

        // Even without the static_assert above, this will produce an error on
        // unsigned types, because the cast to a signed type implied by - (by
        // way of abs) is ambiguous.  see abs()
        return from_scaled(-content);
    }

    fixedp& operator++()
    {
        content += Scale;
        return *this;
    }
    fixedp operator++(int) // postfix
    {
        fixedp tmp(*this);
        operator++();
        return tmp;
    }
    fixedp& operator--()
    {
        content -= Scale;
        return *this;
    }
    fixedp operator--(int)
    {
        fixedp tmp(*this);
        operator--();
        return tmp;
    }

    fixedp& operator+=(const fixedp& rhs)
    {
        content += rhs.content;
        return *this;
    }
    inline friend fixedp operator+(fixedp lhs, const fixedp& rhs)
    {
        lhs += rhs; // lhs is a copy
        return lhs;
    }

    fixedp& operator-=(const fixedp& rhs)
    {
        content -= rhs.content;
        return *this;
    }
    inline friend fixedp operator-(fixedp lhs, const fixedp& rhs)
    {
        lhs -= rhs; // lhs is a copy
        return lhs;
    }

    fixedp& operator*=(const fixedp& rhs)
    {
        if (rhs.frac_part() == 0)
            content *= rhs.integral_part();
        else
        {
            // TODO: better multiplication algorithm?
            content *= rhs.content;
            if (rounding)
            {
                // exploit truncation to do rounding
                content = (content > 0) ? content + Scale / 2
                                        : content - Scale / 2;
            }
            // content now is scaled by Scale * Scale, so we need to divide
            // one out.
            content /= Scale;
        }
        return *this;
    }
    inline friend fixedp operator*(fixedp lhs, const fixedp& rhs)
    {
        lhs *= rhs; // lhs is a copy
        return lhs;
    }

    fixedp& operator/=(const fixedp& rhs)
    {
        // TODO: this is probably a bad way to do this
        content *= Scale;
        if (rounding)
        {
            // exploit truncation to do rounding
            content = (content >= 0) ? content + _safe_abs(rhs.content) / 2
                                     : content - _safe_abs(rhs.content) / 2;
        }
        content /= rhs.content;
        return *this;
    }
    inline friend fixedp operator/(fixedp lhs, const fixedp& rhs)
    {
        lhs /= rhs; // lhs is a copy
        return lhs;
    }

#ifdef ASSERT
// crawlcode
#   define FPASSERT ASSERT
#else
#   define FPASSERT assert
#endif

    static void test_cases()
    {
        // exercise constructors
        fixedp<int, 100> test_dbl((double) 10.629);
        int int_test;
        int_test = 292;
        fixedp<> test_int(int_test);
        fixedp<long> test_long((long) 2e10);
        FPASSERT(test_long == 2e10);

        // test some cv qualified fixedps
        const fixedp<> cv_test(500, 20);
        // no idea why you'd want a volatile fixedp in crawl, but you can
        // uncomment these to make sure it would compile.
        // volatile fixedp<> cv_test2(500, 20);
        // const volatile fixedp<> cv_test3(500, 20);
        const fixedp<> cv_test4 = cv_test * 2;
        FPASSERT(cv_test == 500.2);
        FPASSERT(cv_test * 2 == cv_test4);
        FPASSERT(cv_test.round() == 500);
        FPASSERT(abs(cv_test * -1) == 500.2);
        // shouldn't compile
        // cv_test *= 2;

        // test assignment operators
        fixedp<int, 100> test((float) 10.629);
        FPASSERT(test == 10.63);
        FPASSERT(test == 10.629);
        test += 10;
        FPASSERT(test == 20.63);
        test += 10.2;
        FPASSERT(test == 30.83);
        test *= 2;
        FPASSERT(test == 61.66);
        test *= 2.5;
        FPASSERT(test == 154.15);
        test /= 2.5;
        FPASSERT(test == 61.66);
        test -= 102;
        FPASSERT(test == -40.34);
        test *= 2;
        FPASSERT(test == -80.68);
        FPASSERT(-test == 80.68);
        test = 96.236;
        FPASSERT(test.to_scaled() == 9624);

        // exercise every comparison operator for both positive and negative
        FPASSERT(test == 96.24);
        FPASSERT(test >= 96.24);
        FPASSERT(test <= 96.24);
        FPASSERT(test >= 96);
        FPASSERT(test > 96);
        FPASSERT(test != 96);
        FPASSERT(test < 97);
        FPASSERT(test <= 97);
        test = -96.236;
        FPASSERT(test == -96.24);
        FPASSERT(test >= -96.24);
        FPASSERT(test <= -96.24);
        FPASSERT(test <= -96);
        FPASSERT(test < -96);
        FPASSERT(test != -96);
        FPASSERT(test != 96);
        FPASSERT(test > -97);
        FPASSERT(test >= -97);

        FPASSERT(fixedp<>(-10.23).frac_part() == fixedp<>(-10.23).frac_part());
        FPASSERT(fixedp<>(-10.23).frac_part()
                == static_cast<int>(fixedp<unsigned int>(10.23).frac_part()));

        // testing rounding and other math operators
        FPASSERT(abs(fixedp<>(-10)) == 10);
        FPASSERT(abs(fixedp<>(10)) == 10);

        FPASSERT(fixedp<>(2.6).round() == 3);
        FPASSERT(fixedp<>(2.6).round() != 2);
        FPASSERT(fixedp<>(2.4).round() == 2);
        FPASSERT(fixedp<>(2.4).round() != 3);
        FPASSERT(fixedp<>(-2.6).round() == -3);
        FPASSERT(fixedp<>(-2.6).round() != -2);
        FPASSERT(fixedp<>(-2.4).round() == -2);
        FPASSERT(fixedp<>(-2.4).round() != -3);

        FPASSERT(ceil(fixedp<>(2.6)) == 3);
        FPASSERT(ceil(fixedp<>(2.6)) != 2);
        FPASSERT(ceil(fixedp<>(2.4)) == 3);
        FPASSERT(ceil(fixedp<>(2.4)) != 2);
        FPASSERT(ceil(fixedp<>(-2.6)) == -2);
        FPASSERT(ceil(fixedp<>(-2.6)) != -3);
        FPASSERT(ceil(fixedp<>(-2.4)) == -2);
        FPASSERT(ceil(fixedp<>(-2.4)) != -3);

        FPASSERT(floor(fixedp<>(2.6)) == 2);
        FPASSERT(floor(fixedp<>(2.6)) != 3);
        FPASSERT(floor(fixedp<>(2.4)) == 2);
        FPASSERT(floor(fixedp<>(2.4)) != 3);
        FPASSERT(floor(fixedp<>(-2.6)) == -3);
        FPASSERT(floor(fixedp<>(-2.6)) != -2);
        FPASSERT(floor(fixedp<>(-2.4)) == -3);
        FPASSERT(floor(fixedp<>(-2.4)) != -2);

        FPASSERT(fixedp<>(2.6).trunc() == 2);
        FPASSERT(fixedp<>(2.6).trunc() != 3);
        FPASSERT(fixedp<>(2.4).trunc() == 2);
        FPASSERT(fixedp<>(2.4).trunc() != 3);
        FPASSERT(fixedp<>(-2.6).trunc() == -2);
        FPASSERT(fixedp<>(-2.6).trunc() != -3);
        FPASSERT(fixedp<>(-2.4).trunc() == -2);
        FPASSERT(fixedp<>(-2.4).trunc() != -3);

        FPASSERT(min(fixedp<>(5), fixedp<>(5.5)) == 5);
        FPASSERT(min(fixedp<>(-5), fixedp<>(5.5)) == -5);
        FPASSERT(min(fixedp<>(5), fixedp<>(-5.5)) == -5.5);
        FPASSERT(min(fixedp<>(-5), fixedp<>(-5.5)) == -5.5);
        FPASSERT(max(fixedp<>(5), fixedp<>(5.5)) == 5.5);
        FPASSERT(max(fixedp<>(-5), fixedp<>(5.5)) == 5.5);
        FPASSERT(max(fixedp<>(5), fixedp<>(-5.5)) == 5);
        FPASSERT(max(fixedp<>(-5), fixedp<>(-5.5)) == -5);
        // implicit cast examples for min/max
        FPASSERT(min(fixedp<>(5), 5.5) == 5);
        FPASSERT(max(fixedp<>(5), 5.5) == 5.5);

        // test casts
        FPASSERT(fixedp<>(10, 20) == fixedp<>(10.2));
        FPASSERT((bool) fixedp<>(1));
        FPASSERT((int) fixedp<>(1.2) == 1);
        FPASSERT((int) fixedp<>(-1.6) == -1);
        FPASSERT((long) fixedp<>(1.2) == 1);
        FPASSERT((long) fixedp<>(-1.6) == -1);
        FPASSERT((float) fixedp<>(1.2) > 1.1888);
        FPASSERT((float) fixedp<>(-1.6) < -1.1899);

        // test binary arithmetic
        FPASSERT(fixedp<>(15) + fixedp<>(12.2) == fixedp<>(27.2));
        FPASSERT(fixedp<>(15) - fixedp<>(12.2) == fixedp<>(2.8));
        FPASSERT(fixedp<>(15) * fixedp<>(12.2) == fixedp<>(183));
        FPASSERT(fixedp<>(183) / fixedp<>(12.2) == fixedp<>(15));

        // arithmetic with implicit casts to fixedp
        FPASSERT(fixedp<>(15) + 12.2 == fixedp<>(27.2));
        FPASSERT(fixedp<>(15) - 12.2 == fixedp<>(2.8));
        FPASSERT(fixedp<>(15) * 12.2 == fixedp<>(183));
        FPASSERT(fixedp<>(183) / 12.2 == fixedp<>(15));
        FPASSERT(15 + fixedp<>(12.2) == fixedp<>(27.2));
        FPASSERT(15 - fixedp<>(12.2) == fixedp<>(2.8));
        FPASSERT(15 * fixedp<>(12.2) == fixedp<>(183));
        FPASSERT(183 / fixedp<>(12.2) == fixedp<>(15));

        FPASSERT((fixedp<int, 10>(11) / 10 == 1.1));
        FPASSERT((fixedp<int, 10>(-11) / 10 == -1.1));
        FPASSERT((fixedp<int, 10>(11) / -10 == -1.1));
        FPASSERT((fixedp<int, 10>(-11) / -10 == 1.1));


        // test rounding and truncation behavior for /,*
        test = fixedp<>(116.66);
        auto test2 = fixedp<>(114.44);
        fixedp<>::set_rounding(false);
        FPASSERT((test * 100) / 100 == test);
        FPASSERT((test * 0.01) * 100 == 116); // loses 2 digits
        FPASSERT((test / 100) * 100 == 116);
        FPASSERT((test2 * 0.01) * 100 == 114);
        FPASSERT((test2 / 100) * 100 == 114);
        FPASSERT((test * 0.01) == (test / 100));
        FPASSERT((test / 0.01) == (test * 100));
        fixedp<>::set_rounding(true);
        FPASSERT((test * 100) / 100 == test);
        FPASSERT((test * 0.01) * 100 == 117); // loses 2 digits, rounds
        FPASSERT((test / 100) * 100 == 117);
        FPASSERT((test2 * 0.01) * 100 == 114);
        FPASSERT((test2 / 100) * 100 == 114);
        FPASSERT((test * 0.01) == (test / 100));
        FPASSERT((test / 0.01) == (test * 100));
        // full combination of rounding + signs
        FPASSERT((test * -0.01) * 100 == -117);
        FPASSERT((-test * 0.01) * 100 == -117);
        FPASSERT((-test * -0.01) * 100 == 117);
        FPASSERT((test / -100) * 100 == -117);
        FPASSERT((-test / 100) * 100 == -117);
        FPASSERT((-test / -100) * 100 == 117);

        // comparison with explicit casts to fixedp
        // didn't bother to do this thoroughly
        FPASSERT(15.0 + 12.2 == fixedp<>(27.2));
        FPASSERT(15.0 - 12.2 == fixedp<>(2.8));
        FPASSERT(15.0 * 12.2 == fixedp<>(183));
        FPASSERT(183.0 / 12.2 == fixedp<>(15));

        // test conversions between fixedps.
        // these need the double parens because `assert` gets confused with too
        // many commas.
        FPASSERT((fixedp<int, 100>::from_fixedp<10>(fixedp<int, 10>(10)) == 10));
        FPASSERT((fixedp<int, 100>::from_fixedp<1000>(
                                    fixedp<int, 1000>(10)) == 10));
        FPASSERT((fixedp<int, 100>::from_fixedp<10>(
                                    fixedp<int, 10>(11) / 10) == 1.1));
        FPASSERT((fixedp<int, 100>::from_fixedp<10>(
                                    fixedp<int, 10>(-11) / 10) == -1.1));
        FPASSERT((fixedp<int, 100>::from_fixedp<1000>(
                                    fixedp<int, 1000>(1111) / 1000) == 1.11));
        // test truncation behavior
        FPASSERT((fixedp<int, 100>::from_fixedp<1000>(
                                    fixedp<int, 1000>(1116) / 1000) == 1.12));

        FPASSERT((fixedp<int, 100>(fixedp<int, 10>(10)) == 10));
        FPASSERT((fixedp<int, 100>(fixedp<int, 1000>(10)) == 10));
        FPASSERT((fixedp<int, 100>(fixedp<int, 10>(11) / 10) == 1.1));
        FPASSERT((fixedp<int, 100>(fixedp<int, 10>(-11) / 10) == -1.1));
        FPASSERT((fixedp<int, 100>(fixedp<int, 1000>(1111) / 1000) == 1.11));
        FPASSERT((fixedp<int, 100>(fixedp<int, 1000>(1116) / 1000) == 1.12));


        // test a few max scales; these have only a fractional part
        fixedp<char, 127>(10);
        fixedp<unsigned char, 255>(10);

        // these should cause a static_assert failure if uncommented:
        // fixedp<char, 16000>(10);
        // fixedp<char, 128>(10);
        // fixedp<unsigned char, 256>(10);
        // float test_badneg = (float) -fixedp<unsigned int>(10,20);
        // float test_badabs = (float) abs(fixedp<unsigned int>(10,20));
        // fixedp<int, -100> test_badscale(100);
    }
#undef FPASSERT

};

// sources vary on whether you are allowed to and/or should do this. But we
// want a way of getting these values somehow, and unless/until some c++
// version prevents me from doing it, I'd rather use the standard API if at
// all possible.
namespace std
{
    template<typename BaseType, int Scale>
    class numeric_limits<fixedp<BaseType, Scale>>
    {
    public:
        static fixedp<BaseType, Scale> min()
        {
            return fixedp<BaseType, Scale>::from_scaled(numeric_limits<BaseType>::min());
        }

        static fixedp<BaseType, Scale> max()
        {
            return fixedp<BaseType, Scale>::from_scaled(numeric_limits<BaseType>::max());
        }
    };
}

// round by default
template <typename BaseType, int Scale> bool
                                    fixedp<BaseType, Scale>::rounding = true;

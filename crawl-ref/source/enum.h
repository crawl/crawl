/**
 * @file
 * @brief Global (ick) enums.
**/

#pragma once

#include <iterator>
#include <type_traits> // underlying_type<>, enable_if<>

// Provide a last_exponent static member variable only if the LastExponent
// template parameter is nonnegative.
template<int LastExponent, bool Provided = LastExponent >= 0>
struct _enum_bitfield_exponent_base { };

template<int LastExponent>
struct _enum_bitfield_exponent_base<LastExponent, true>
{
    static constexpr int last_exponent = LastExponent;
};

// If LastExponent is nonnegative, is the last exponent that will
// be iterated over by range()
template<class E, int LastExponent = -1>
class enum_bitfield : public _enum_bitfield_exponent_base<LastExponent>
{
public:
    typedef typename underlying_type<E>::type underlying_type;
    typedef enum_bitfield<E, LastExponent> field_type;
    underlying_type flags;
private:
    explicit constexpr enum_bitfield(underlying_type rawflags)
        : flags(rawflags)
    {}
public:
    /// Get the flag corresponding to the given bit position (0 = LSB).
    static constexpr E exponent(int pos)
    {
        return static_cast<E>(underlying_type(1) << pos);
    }

    constexpr enum_bitfield() : flags(0) {}
    constexpr enum_bitfield(E flag) : flags(underlying_type(flag)) {}
    template<class ... Es>
    constexpr enum_bitfield(E flag, E flag2, Es... rest)
        : flags(enum_bitfield(flag2, rest...).flags | underlying_type(flag)) {}

    explicit constexpr operator underlying_type () const { return flags; }
    explicit constexpr operator bool () const { return flags; }
    constexpr bool operator==(field_type other) const
    {
        return flags == other.flags;
    }
    constexpr bool operator!=(field_type other) const
    {
        return !(*this == other);
    }

    field_type &operator|=(field_type other)
    {
        flags |= other.flags;
        return *this;
    }

    field_type &operator&=(field_type other)
    {
        flags &= other.flags;
        return *this;
    }

    field_type &operator^=(field_type other)
    {
        flags ^= other.flags;
        return *this;
    }

    constexpr field_type operator|(field_type other) const
    {
        return field_type(flags | other.flags);
    }

    constexpr field_type operator&(field_type other) const
    {
        return field_type(flags & other.flags);
    }

    constexpr field_type operator^(field_type other) const
    {
        return field_type(flags ^ other.flags);
    }

    constexpr field_type operator~() const
    {
        return field_type(~flags);
    }

    struct range
    {
        class iterator : public std::iterator<forward_iterator_tag, const E>
        {
        private:
            int exp_;
        public:
            constexpr iterator() : exp_(0) { }
            constexpr iterator(int exp) : exp_(exp) { }
            constexpr E operator*() const { return exponent(exp_); }
            constexpr bool operator==(const iterator &other) const
            {
                return exp_ == other.exp_;
            }
            constexpr bool operator!=(const iterator &other) const
            {
                return !(*this == other);
            }
            iterator &operator++() { ++exp_; return *this; }
            iterator operator++(int)
            {
                iterator copy = *this;
                ++(*this);
                return copy;
            }
        };

        const int final;

        constexpr range(int last_exp) : final(last_exp) {}

        // Only create the default constructor if we got a nonnegative
        // LastExponent template parameter.
        template<typename enable_if<LastExponent >= 0, int>::type = 0>
        constexpr range() : final(LastExponent) {}

        constexpr iterator begin() const { return iterator(); }
        constexpr iterator end() const { return final + 1; }
    };
};

#define DEF_BITFIELD_OPERATORS(fieldT, flagT, ...) \
    inline constexpr fieldT operator|(flagT a, flagT b)  { return fieldT(a) | b; } \
    inline constexpr fieldT operator|(flagT a, fieldT b) { return fieldT(a) | b; } \
    inline constexpr fieldT operator&(flagT a, flagT b)  { return fieldT(a) & b; } \
    inline constexpr fieldT operator&(flagT a, fieldT b) { return fieldT(a) & b; } \
    inline constexpr fieldT operator^(flagT a, flagT b)  { return fieldT(a) ^ b; } \
    inline constexpr fieldT operator^(flagT a, fieldT b) { return fieldT(a) ^ b; } \
    inline constexpr fieldT operator~(flagT a) { return ~fieldT(a); } \
    COMPILE_CHECK(is_enum<flagT>::value)
// The last line above is really just to eat a semicolon; template
// substitution of enum_bitfield would have already failed.

// Work around MSVC's idiosyncratic interpretation of __VA_ARGS__ as a
// single macro argument: http://stackoverflow.com/questions/5134523
#define EXPANDMACRO(x) x
/**
 * Define fieldT as a bitfield of the enum flagT, and make bitwise
 * operators on flagT yield a fieldT.
 *
 * This macro produces a typedef and several inline function definitions;
 * use it only at file/namespace scope. It requires a trailing semicolon.
 *
 * The operations ~, |, ^, and (binary) & on flags or fields yield a field.
 * Fields also support &=, |=, and ^=. Fields can be explicitly converted to
 * an integer of the enum's underlying type, or to bool. Note that in C++
 * using a bitfield expression as the condition of a control structure,
 * or as an operand of a logical operator, counts as explicit conversion
 * to bool.
 *
 * @param fieldT   An identifier naming the bitfield type to define.
 * @param flagT    An identifier naming the enum type to use as a flag.
 *                 Could theoretically be a more complex type expression, but
 *                 I wouldn't try anything trickier than scope resolution.
 * @param lastExp  The last exponent over which fieldT::range() should
 *                 iterate. If unspecified or negative, the bitfield will not
 *                 have the last_exponent static member variable, and the
 *                 bitfield's nested range class will not have a default
 *                 constructor.
 */
#define DEF_BITFIELD(fieldT, ...) \
    typedef enum_bitfield<__VA_ARGS__> fieldT; \
    EXPANDMACRO(DEF_BITFIELD_OPERATORS(fieldT, __VA_ARGS__, ))
// The trailing comma suppresses "ISO C99 requires rest arguments to be used"

// The rest of this file should probably move elsewhere
// normal tile size in px
enum
{
    TILE_X = 32,
    TILE_Y = 32,
};

// Don't change this without also modifying the data save/load routines.
enum
{
    NUM_MAX_DOLLS = 10,
};


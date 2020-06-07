/**
 * @file
 * @brief Representation of a coordinate pair (usually an on-map location).
**/

#pragma once

// Constexpr sign.
template <typename Z> static constexpr Z sgn(Z x)
{
    return x < 0 ? -1 : (x > 0 ? 1 : 0);
}

// Constexpr absolute value.
template <typename Z> static constexpr Z abs_ce(Z x)
{
    return x < 0 ? -x : x;
}

struct coord_def
{
    int         x;
    int         y;

    constexpr coord_def(int x_in, int y_in) : x(x_in), y(y_in) { }
    constexpr coord_def() : coord_def(0,0) { }

    void set(int xi, int yi)
    {
        x = xi;
        y = yi;
    }

    void reset()
    {
        set(0, 0);
    }

    constexpr int distance_from(const coord_def &b) const
    {
        return (b - *this).rdist();
    }

    constexpr bool operator == (const coord_def &other) const
    {
        return x == other.x && y == other.y;
    }

    constexpr bool operator != (const coord_def &other) const
    {
        return !operator == (other);
    }

    constexpr bool operator <  (const coord_def &other) const
    {
        return x < other.x || (x == other.x && y < other.y);
    }

    constexpr bool operator >  (const coord_def &other) const
    {
        return x > other.x || (x == other.x && y > other.y);
    }

    constexpr bool operator<= (const coord_def &other) const
    {
        return operator== (other) || operator< (other);
    }

    constexpr bool operator>= (const coord_def &other) const
    {
        return operator== (other) || operator> (other);
    }

    const coord_def &operator += (const coord_def &other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    const coord_def &operator += (int offset)
    {
        x += offset;
        y += offset;
        return *this;
    }

    const coord_def &operator -= (const coord_def &other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    const coord_def &operator -= (int offset)
    {
        x -= offset;
        y -= offset;
        return *this;
    }

    const coord_def &operator /= (int div)
    {
        x /= div;
        y /= div;
        return *this;
    }

    const coord_def &operator *= (int mul)
    {
        x *= mul;
        y *= mul;
        return *this;
    }

    constexpr coord_def operator + (const coord_def &other) const
    {
        return { x + other.x, y + other.y };
    }

    constexpr coord_def operator + (int other) const
    {
        return *this + coord_def(other, other);
    }

    constexpr coord_def operator - (const coord_def &other) const
    {
        return { x - other.x, y - other.y };
    }

    constexpr coord_def operator -() const
    {
        return { -x, -y };
    }

    constexpr coord_def operator - (int other) const
    {
        return *this - coord_def(other, other);
    }

    constexpr coord_def operator / (int div) const
    {
        return { x / div, y / div };
    }

    constexpr coord_def operator * (int mul) const
    {
        return { x * mul, y * mul };
    }

    constexpr coord_def sgn() const
    {
        return coord_def(::sgn(x), ::sgn(y));
    }

    constexpr int abs() const
    {
        return x * x + y * y;
    }

    constexpr int rdist() const
    {
        // Replace with max(abs_ce(x), abs_ce(y) when we require C++14.
        return abs_ce(x) > abs_ce(y) ? abs_ce(x) : abs_ce(y);
    }

    constexpr bool origin() const
    {
        return !x && !y;
    }

    constexpr bool zero() const
    {
        return origin();
    }

    constexpr bool equals(const int xi, const int yi) const
    {
        return *this == coord_def(xi, yi);
    }

    const coord_def clamped(const coord_def &tl, const coord_def &br) const
    {
        return coord_def(min(max(x, tl.x), br.x), min(max(y, tl.y), br.y));
    }

    const coord_def clamped(const pair<coord_def, coord_def> &bounds) const
    {
        return clamped(bounds.first, bounds.second);
    }
};

typedef pair<coord_def, int> coord_weight;

inline ostream& operator << (ostream& ostr, coord_def const& value)
{
    ostr << "coord_def(" << value.x << ", " << value.y << ")";
    return ostr;
}

namespace std {
    template <>
    struct hash<coord_def>
    {
        constexpr size_t operator()(const coord_def& c) const
        {
            // lazy assumption: no coordinate will ever be bigger than 2^16
            return (c.x << 16) + c.y;
        }
    };
}

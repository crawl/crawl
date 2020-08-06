/**
 * @file
 * @brief Monster action goodness rating type and associated operations.
**/

namespace ai_action
{
    /// Goodness values for actions.
    /// First coordinate is whether the action is possible at all.
    /// Second coordinate is a measure of goodness: higher
    /// is better, negative means prefer not to do. Currently, you should
    /// mostly ignore the actual values and use comparison with bad(), good(),
    /// and neutral() below. This way, you can basically treat them as very
    /// fancy bools, with an extra way of marking actions as impossible.
    /// comparison using >, <, == works as expected: better values are higher,
    /// and impossible actions are the least element. Right now, very little
    /// else is imposed on the scale, except that anything >= is considered to
    /// be "viable".
    ///
    /// `std::min` and `std::max` work on these as well: for &&-like logic use
    /// `min`, for ||-like logic use `max`.
    typedef const pair<bool,int> goodness; // TODO: replace with optional<int> in c++17

    /// Goodness value for an impossible action.
    goodness impossible() { return goodness(false, INT_MIN); }
    /// Goodness values for possible actions of goodness `g`.
    goodness possible(int g) { return goodness(true, g); }
    /// Goodness value for a bad action (that is possible)
    goodness bad() { return possible(-100); }
    /// The worst possible goodness value.
    goodness worst() { return possible(INT_MIN); }
    /// Goodness value for a good action.
    goodness good() { return possible(100); }
    /// Goodness value for a neutral action.
    goodness neutral() { return possible(0); }

    /// Convert a bool to a goodness value by mapping true to good, and false
    /// to bad.
    goodness good_or_bad(bool b) { return b ? good() : bad(); }

    /// Convert a bool to a goodness value by mapping true to good, and false
    /// to impossible.
    goodness good_or_impossible(bool b) { return b ? good() : impossible(); }

    /// Is the action considered a possible action? This is a very low bound,
    /// and basically means the action won't crash.
    /// even INT_MIN counts as possible, as long as the first coordinate is true.
    bool is_possible(goodness g) { return g >= worst(); }

    /// Is the action viable? This is true if it is neutral or better.
    // TODO: allow adjusting this threshold? Would be useful at least for tests.
    bool is_viable(goodness g) { return g >= neutral(); }
}

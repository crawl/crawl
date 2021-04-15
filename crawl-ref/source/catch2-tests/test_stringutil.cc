#include "catch.hpp"
#include <vector>
#include <list>
#include <functional>

#include "stringutil.h"

// Test plain arrays, vectors, and lists (not random-access), with const variants of both
TEMPLATE_TEST_CASE( "comma_separated_*", "[single-file]",
                    const char *[], string[], const string[],
                    vector<const char *>, const vector<string>,
                    list<const char *>, const list<const char *> )
{
    TestType several = { "foo", "bar", "baz" };
    TestType two = { "foo", "bar" };
    TestType one = { "foo" };

    CHECK(comma_separated_line(begin(several), end(several)) == "foo, bar and baz");
    CHECK(comma_separated_line(begin(two), end(two)) == "foo and bar");
    CHECK(comma_separated_line(begin(one), end(one)) == *begin(one));
    // N.b. begin() twice is intentional, to get an empty range (likewise below)
    CHECK(comma_separated_line(begin(one), begin(one)) == "");

    CHECK(comma_separated_line(begin(several), end(several), "&", "+") == "foo+bar&baz");
    CHECK(comma_separated_line(begin(two), end(two), "&", "+") == "foo&bar");
    CHECK(comma_separated_line(begin(one), end(one), "&", "+") == *begin(one));
    CHECK(comma_separated_line(begin(one), begin(one), "&", "+") == "");

    CHECK(join_strings(begin(several), end(several)) == "foo bar baz");
    CHECK(join_strings(begin(two), end(two)) == "foo bar");
    CHECK(join_strings(begin(one), end(one)) == *begin(one));
    CHECK(join_strings(begin(one), begin(one)) == "");

    CHECK(comma_separated_fn(begin(several), end(several), uppercase_string)
            == "FOO, BAR and BAZ");
    CHECK(comma_separated_fn(begin(two), end(two), uppercase_string) == "FOO and BAR");
    CHECK(comma_separated_fn(begin(one), end(one), uppercase_string)
            == uppercase_string(*begin(one)));
    CHECK(comma_separated_fn(begin(one), begin(one), uppercase_string) == "");

    CHECK(comma_separated_fn(begin(several), end(several), uppercase_string, "&", "+")
            == "FOO+BAR&BAZ");
    CHECK(comma_separated_fn(begin(two), end(two), uppercase_string, "&", "+") == "FOO&BAR");
    CHECK(comma_separated_fn(begin(one), end(one), uppercase_string, "&", "+")
            == uppercase_string(*begin(one)));
    CHECK(comma_separated_fn(begin(one), begin(one), uppercase_string, "&", "+") == "");

}

TEST_CASE( "comma_separated_fn with a non-string", "[single-file]")
{
    int numlist[] = { 1, 2, 3, 4 };
    // Select a specific overload of std::to_string.
    string (*fn)(int) = to_string;
    // Also with a std::function
    function<string(int)> fn_func{fn};

    CHECK(comma_separated_fn(begin(numlist), end(numlist), fn) == "1, 2, 3 and 4");
    CHECK(comma_separated_fn(begin(numlist), end(numlist), fn_func) == "1, 2, 3 and 4");
}

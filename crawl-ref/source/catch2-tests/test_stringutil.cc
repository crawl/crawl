#include "catch_amalgamated.hpp"
#include <vector>
#include <list>
#include <functional>
#include <cstring>
#include <locale.h>

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

TEST_CASE( "uppercase and lowercase", "[single-file]")
{
    for (int i = 0; i < 2; ++i) {
        // Also try with a Turkish locale, where tolower("I") is not "i". Our uppercase
        // and lowercase functions should ignore the locale for ASCII characters, because
        // they are also used for things that aren't exactly "text".
        if (i > 0)
            setlocale(LC_ALL, "tr_TR");

        // N.b. includes a capital I
        const char orig[] = "mIxEdCaSe";
        string in_s(orig);
        const string in_cs(orig);
        const char *in = in_s.c_str();

        // First, the non-mutating versions.
        CHECK(lowercase_string(in) == "mixedcase");
        CHECK(lowercase_string(in_s) == "mixedcase");
        CHECK(lowercase_string(in_cs) == "mixedcase");

        // Ensure they didn't mutate the input, and abort the test case if they did.
        REQUIRE(in_s == orig);
        REQUIRE(strcmp(in, orig) == 0);

        // Again for uppercasing
        CHECK(uppercase_string(in) == "MIXEDCASE");
        CHECK(uppercase_string(in_s) == "MIXEDCASE");
        CHECK(uppercase_string(in_cs) == "MIXEDCASE");

        REQUIRE(in_s == orig);
        REQUIRE(strcmp(in, orig) == 0);

        // Again for title-casing
        CHECK(uppercase_first(in) == "MIxEdCaSe");
        CHECK(uppercase_first(in_s) == "MIxEdCaSe");
        CHECK(uppercase_first(in_cs) == "MIxEdCaSe");

        REQUIRE(in_s == orig);
        REQUIRE(strcmp(in, orig) == 0);

        // Now the mutating versions
        string s1(orig);
        string &result = uppercase(s1);
        CHECK(result == "MIXEDCASE");
        // Verify identity
        CHECK(&result == &s1);

        string &result2 = lowercase(result);
        CHECK(result2 == "mixedcase");
        // Verify identity
        CHECK(&result2 == &s1);
    }
}

#include "catch.hpp"
#include <vector>
#include <list>

#include "stringutil.h"

// Test plain arrays, vectors, and lists (not random-access)
TEMPLATE_TEST_CASE( "comma_separated_line", "[single-file]",
                    const char *[], vector<const char *>, const vector<string>,
                    list<const char *>, const list<const char *> )
{
    // TODO: also test with 
    TestType several = { "foo", "bar", "baz" };
    TestType two = { "foo", "bar" };
    TestType one = { "foo" };

    CHECK(comma_separated_line(begin(several), end(several)) == "foo, bar and baz");
    CHECK(comma_separated_line(begin(two), end(two)) == "foo and bar");
    CHECK(comma_separated_line(begin(one), end(one)) == *begin(one));
    CHECK(comma_separated_line(begin(one), begin(one)) == "");
    
    CHECK(join_strings(begin(several), end(several)) == "foo bar baz");
    CHECK(join_strings(begin(two), end(two)) == "foo bar");
    CHECK(join_strings(begin(one), end(one)) == *begin(one));
    CHECK(join_strings(begin(one), begin(one)) == "");

}

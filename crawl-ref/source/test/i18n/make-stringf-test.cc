#include <string>
#include <cinttypes>
#include <climits>
#include <iostream>
using namespace std;

#include "AppHdr.h"
#include "random.h" // for random2()
#include "stringutil.h"

// dummy function to satisfy linker
NORETURN void AssertFailed(const char*, const char*, int ,
                           const char*, ...)
{
    abort();
}

// dummy function to satisfy linker
#undef die
NORETURN void die(const char*, int, const char*, ...)
{
    abort();
}

// dummy function to satisfy linker
int random2(int max)
{
    return max;
}

int main()
{
    cout << make_stringf("Hello!") << endl;
    cout << make_stringf("Signed: char: %hhd, short: %hd, int: %d, long: %ld, long long: %lld", (char)-10, (short)-42, INT_MIN, LONG_MIN, (long long)LONG_MIN) << endl;
    cout << make_stringf("Signed: PRId32: %" PRId32 ", PRId64: %" PRId64, (int32_t)0x80000000, (int64_t)0x8000000000000000) << endl;
    cout << make_stringf("Unsigned: char: %hhu, short: %hu, int: %u, long: %lu, long long: %llu", (unsigned char)10, (short)42, UINT_MAX, ULONG_MAX, (unsigned long long)ULONG_MAX) << endl;
    cout << make_stringf("Unsigned: PRIu32: %" PRIu32 ", PRIu64: %" PRIu64, (int32_t)0xFFFFFFFF, (int64_t)0xFFFFFFFFFFFFFFFF) << endl;
    cout << make_stringf("Hex: char: %#hhx, short: %#hx, int: %#x, long: %#lx, long long: %#llx", (unsigned char)10, (short)42, UINT_MAX, ULONG_MAX, (unsigned long long)ULONG_MAX) << endl;
    cout << make_stringf("Hex: PRIX32: %#" PRIX32 ", PRIX64: %#" PRIX64, (int32_t)0xFFFFFFFF, (int64_t)0xFFFFFFFFFFFFFFFF) << endl;
    cout << make_stringf("float: %f, double: %+f, long double: %.15Lf", (float)-12.345, (double)2.71828, 3.141592653589793L) << endl;

    cout << make_stringf("escaped percent: %%, char: %c, string: \"%s\"", 'A', "Hi!") << endl;

    cout << make_stringf("String with width 5 and precision 4: \"%5.4s\"", "Grüße!") << endl;

    int count = 0;
    cout << make_stringf("And left-justified: \"%-5.4s\"%n", "Grüße!", &count) << endl;
    cout << "Character count for previous output line: " << count << endl;

    int width = 10;
    int precision = 3;
    cout << make_stringf("Dynamic width of 10, dynamic precision of 3, left-justified: \"%-*.*s\"", width, precision, "Grüße!") << endl;

    cout << make_stringf("Explicitly numbered args: %1$.*3$s %2$s", "Grüße!", "Hello!", precision) << endl;

    const wchar_t* wide_string = L"ABCDEFGHIJ";
    cout << make_stringf("Wide string: \"%ls\"", wide_string) << endl;

    return 0;
}

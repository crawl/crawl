#ifndef CTEST_H
#define CTEST_H

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)

namespace crawl_tests
{
    void run_tests(bool exit_on_complete);
}

#endif

#endif

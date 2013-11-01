#ifndef CTEST_H
#define CTEST_H

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
NORETURN void run_tests();
#endif

#endif

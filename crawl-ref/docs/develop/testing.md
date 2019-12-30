# Testing

## Contents

* [Unit Tests](#unit-tests)
  * [Plug & Play / Bisect Testing](#plug-play-bisect-testing)
* [Functional (Lua) Tests](#functional-lua-tests)
* [Arena Testing](#arena-testing)
* [Code Coverage](#code-coverage)

## Unit Tests

Crawl integrates the Catch2 unit testing framework. Tests are in the [crawl-ref/source/catch2-tests](crawl-ref/source/catch2-tests) directory. To compile and run all units tests, use:

```sh
make catch2-tests
```

### Plug & Play / Bisect Testing

`test_plug_and_play.cc` is an optional source file for catch2 tests. If
the source file is available, the `make plug-and-play-tests` command
will use it to create a catch2 tests binary. If the source file is not
available, the `make plug-and-play-tests` command will fail. This source
file is in the .gitignore and should never be added to the git project.

The main use of `test_plug_and_play.cc` is for the file to be used in
conjunction with the `git bisect` command. If you don't know how the git
bisect command works, reference `git bisect --help` because otherwise
none of the following will make sense.

`test_plug_and_play.cc` is a file which:

1. Compilations of versions of crawl older than master can search for
catch2 tests
2. Will not be overwritten by git history changes, such as those which
occur while using git bisect.

This combination of features allows for you to write tests once, then
use them for any version of crawl which has previously existed, all the
way back to the original implementation of catch2 support for crawl.

While you are hunting down a bug introduced in newer versions of crawl,
see if you can create a catch2 test which checks for the bug in
`test_plug_and_play.cc`. Verify this test works with `make
plug-and-play-tests`. Then, see if this test works correctly when you
`git checkout` a version of crawl which does not have the bug. If both
are true, you can use `test_plug_and_play.cc` to easily find the bug.

Simply use git bisect as normally, but compile and run
plug-and-play-tests instead of normal crawl. Whenever the bug is
present, your test will fail and you can mark that commit as bad.
Whenever the bug is absent, your test will pass and you can mark that
commit as good.

This test process is so easy git bisect allows you to automate it. Look
into `git bisect run` for more details.

When you've finished solving the bug, remember to move the new test from
`test_plug_and_play.cc` to a regular test file included in the git
project! This will help prevent regressions.

## Functional (Lua) Tests

Crawl has a set of functional tests in the [source/test/](crawl-ref/source/test/) directory, with each lua
file being a separate unit test. To use unit tests, you must compile Crawl
either with the macro `DEBUG_DIAGNOSTICS` defined (the debug make target) or
with both the macros `DEBUG` and `DEBUG_TESTS` defined (the wizard make target,
plus `-DDEBUG_TESTS`). You can then do

```sh
crawl -test
```

to run all unit tests. To run a particular test, you can do

```sh
crawl -test foo
```

and that will run all unit tests which have "foo" in their file name.

If you want to write your own unit tests, take a look at the files in
[source/test/](crawl-ref/source/test/) for examples. [los_maps.lua](crawl-ref/source/test/los_maps.lua) and [bounce.lua](crawl-ref/source/test/bounce.lua) have
examples which use vaults (maps) which are located in test/des. You
might need to create new Lua functions in the `source/l_*.cc` files if none of
the `crawl.foo()`/`dgn.foo()`/etc functions do what you need.

## Arena Testing

You can use Crawl's arena mode to test a lot of things. See [arena.txt](crawl-ref/docs/develop/arena.txt) for more information.

## Code Coverage

Code coverage instrumentation is included in all debug & unit test builds. You can use it as follows:

1. Compile the game in debug or unit test mode:

```sh
make debug
# or
make catch2-tests
```

2. Run the compiled binary to generate coverage data:

```sh
./crawl -test
# or
# ./catch2-test-executable # no need, Makefile ran this automatically
```

3. Generate the coverage report:

```sh
util/coverage # Add --help to see customisation options
```

### Quirks

1. Coverage data from multiple runs is combined. This can lead to confusing execution counts for source code lines in reports. Use `make clean-coverage` to remove all temporary coverage files (including reports).

### A Quick Comparison of HTML Report Formats

Sub-expression coverage covers the granularity of testing a line of code like this:

```cpp
if (test1() && test2())
```

#### lcov / genhtml

* Included with GCC on many Linux distributions (if not, easy to install)
* No sub-expression coverage (line-coverage only)

#### llvm-cov

* Included with LLVM/Clang
* Partial sub-expression coverage (will detect un-executed sub-expressions)

#### gcovr

* Python 3-based utility, harder to install on mingw and older systems
* Full sub-expression coverage (will detect if each sub-expression was tested with a positive and negative test case)

### Implementation Details

Code coverage is handled very differently between the GCC and LLVM toolchains. The `Makefile` and `util/coverage` both auto-detect your toolchain, so this is abstracted away for general use. However, coverage percentages and details will be slightly different.

#### GCC

1. Compile with `-fprofile-arcs -ftest-coverage`, this generates `.gcno` files (one per source file)
2. When run, your executable creates `.gcda` (one per source file)
3. Use either `lcov`/`genhtml` or `gcovr`:
    * `lcov`/`genhtml`
        1. Parse `gcno`/`gcda` files with `lcov` into a `.info` file
        2. Generate reports with `genhtml`.
    * `gcovr`
        1. Generate reports with `gcovr`.
            * (`gcovr` uses `gcov` to parse `gcno`/`gcda` files.)

#### LLVM

1. Compile with `-fprofile-instr-generate -fcoverage-mapping`
2. When run, your executable creates `default.profraw`
3. Parse this with `llvm-profdata` into a `.profdata` file
4. Generate reports with `llvm-cov`.

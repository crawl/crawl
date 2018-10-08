#pragma once

#include "tag-version.h"
#include <stdexcept>

NORETURN void fail(PRINTF(0, ));
NORETURN void sysfail(PRINTF(0, ));
NORETURN void corrupted(PRINTF(0, ));
void dump_test_fails(string fails, string name);

struct ext_fail_exception : public runtime_error
{
    ext_fail_exception(const string &msg) : runtime_error(msg) {}
    ext_fail_exception(const char *msg) : runtime_error(msg) {}
};

struct corrupted_save : public ext_fail_exception
{
    corrupted_save(const string &msg) : ext_fail_exception(msg) {}
    corrupted_save(const char *msg) : ext_fail_exception(msg) {}
    corrupted_save(const string &msg, save_version v) :
                ext_fail_exception(msg), version(v)
    {
    }

    save_version version; // defaults to -1,-1
};

extern bool CrawlIsCrashing;

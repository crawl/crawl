#ifndef ERRORS_H
#define ERRORS_H

NORETURN void fail(PRINTF(0, ));
NORETURN void sysfail(PRINTF(0, ));
NORETURN void corrupted(PRINTF(0, ));

class ext_fail_exception : public exception
{
public:
    ext_fail_exception(const string &_msg) : msg(_msg) {}
    ~ext_fail_exception() throw() {}
    const string msg;
};

class corrupted_save : public ext_fail_exception
{
public:
    corrupted_save(const string &_msg) : ext_fail_exception(_msg) {}
};

extern bool CrawlIsCrashing;

#endif

#ifndef ERRORS_H
#define ERRORS_H

noreturn void fail(const char *msg, ...);
noreturn void sysfail(const char *msg, ...);

class ext_fail_exception : public std::exception
{
public:
    ext_fail_exception(const std::string &_msg) : msg(_msg) {}
    ~ext_fail_exception() throw() {}
    const std::string msg;
};

extern bool CrawlIsCrashing;

#endif

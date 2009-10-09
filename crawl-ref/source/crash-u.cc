/*
 *  File:       crash-u.cc
 *  Summary:    UNIX specific crash handling functions.
 *  Written by: Matthew Cline
 */

#include "AppHdr.h"
REVISION("$Rev$");

#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

#include <cxxabi.h>
#include <execinfo.h>

#ifdef OSX
#include <dlfcn.h>

typedef int (*backtrace_t)(void * *, int);
typedef char **(*backtrace_symbols_t)(void * const *, int);

// Used to convert from void* to function pointer (without a
// compiler warning).
template <typename TO, typename FROM> TO nasty_cast(FROM f) {
    union {
        FROM f; TO t;
    } u; u.f = f; return u.t;
}
#endif

#include "crash.h"

#include "externs.h"
#include "state.h"
#include "initfile.h"

/////////////////////////////////////////////////////////////////////////////
// Code for printing out debugging info on a crash.
////////////////////////////////////////////////////////////////////////////
static int _crash_signal    = 0;
static int _recursion_depth = 0;

static void _crash_signal_handler(int sig_num)
{
    if (crawl_state.game_crashed)
    {
        if (_recursion_depth > 0)
            return;
        _recursion_depth++;

        fprintf(stderr, "Recursive crash." EOL);

        std::string dir = (!Options.morgue_dir.empty() ? Options.morgue_dir :
                           !SysEnv.crawl_dir.empty()   ? SysEnv.crawl_dir
                                                       : "");

        if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
            dir += FILE_SEPARATOR;

        char name[180];

        sprintf(name, "%scrash-recursive-%s-%d.txt", dir.c_str(),
                you.your_name, (int) time(NULL));

        FILE* file = fopen(name, "w");

        if (file == NULL)
            file = stderr;

        write_stack_trace(file, 0);

        if (file != stderr)
            fclose(file);
        return;
    }

    _crash_signal            = sig_num;
    crawl_state.game_crashed = true;

    // In case the crash dumper is unable to open a file and has to dump
    // to stderr.
#ifndef USE_TILE
    unixcurses_shutdown();
#endif

    do_crash_dump();

    // Now crash for real.
    signal(sig_num, SIG_DFL);
    raise(sig_num);
}

void init_crash_handler()
{
#if defined(USE_UNIX_SIGNALS)

    for (int i = 1; i <= 64; i++)
    {
#ifdef SIGHUP_SAVE
        if (i == SIGHUP)
            continue;
#endif
#ifdef SIGQUIT
        if (i == SIGQUIT)
            continue;
#endif
#ifdef SIGINT
        if (i == SIGINT)
            continue;
#endif
#ifdef SIGCHLD
        if (i == SIGCHLD)
            continue;
#endif
#ifdef SIGTSTP
        if (i == SIGTSTP)
            continue;
#endif
#ifdef SIGCONT
        if (i == SIGCONT)
            continue;
#endif
#ifdef SIGIO
        if (i == SIGIO)
            continue;
#endif
#ifdef SIGPROF
        if (i == SIGPROF)
            continue;
#endif
        if (i == SIGWINCH)
            continue;

        signal(i, _crash_signal_handler);
    }

#endif // if defined(USE_UNIX_SIGNALS)
}

void dump_crash_info(FILE* file)
{
    const char *name = strsignal(_crash_signal);
    if (name == NULL)
        name = "INVALID";

    fprintf(file, "Crash caused by signal #%d: %s" EOL EOL, _crash_signal,
            name);
}

#if (defined(UNIX) || defined(OSX))
void write_stack_trace(FILE* file, int ignore_count)
{
    void* frames[50];

#if defined (OSX)
    backtrace_t backtrace;
    backtrace_symbols_t backtrace_symbols;
    backtrace = nasty_cast<backtrace_t, void*>(dlsym(RTLD_DEFAULT, "backtrace"));
    backtrace_symbols = nasty_cast<backtrace_symbols_t, void*>(dlsym(RTLD_DEFAULT, "backtrace_symbols"));
    if (!backtrace || !backtrace_symbols)
    {
        fprintf(stderr, "Couldn't get a stack trace." EOL);
        fprintf(file, "Couldn't get a stack trace." EOL);
        return;
    }
#endif

    int num_frames = backtrace(frames, ARRAYSZ(frames));
    char **symbols = backtrace_symbols(frames, num_frames);

#ifndef OSX
    if (symbols == NULL)
    {
        fprintf(stderr, "Out of memory." EOL);
        fprintf(file,   "Out of memory." EOL);

        // backtrace_symbols_fd() can print out the stack trace even if
        // malloc() can't find any free memory.
        backtrace_symbols_fd(frames, num_frames, fileno(file));
        return;
    }
#endif

    fprintf(file, "Obtained %d stack frames." EOL, num_frames);

    // Now we prettify the printout to even show demangled C++ function names.
    std::string bt = "";
    for (int i = 0; i < num_frames; i++) {
#if defined (OSX)
        char *addr = ::strstr(symbols[i], "0x");
        char *mangled = ::strchr(addr, ' ') + 1;
        char *offset = ::strchr(addr, '+');
        char *postmangle = ::strchr(mangled, ' ');
        if (mangled)
            *(mangled - 1) = 0;
        bt += addr;
        int status;
        bt += ": ";
        if (addr && mangled)
        {
            if (postmangle)
                *postmangle = '\0';
            char *realname = abi::__cxa_demangle(mangled, 0, 0, &status);
            if (realname)
                bt += realname;
            else
                bt += mangled;
            bt += " ";
            bt += offset;
            free(realname);
        }
#else // OSX
        bt += symbols[i];
        int status;
        // Extract the identifier from symbols[i].  It's inside of parens.
        char *firstparen = ::strchr(symbols[i], '(');
        char *lastparen = ::strchr(symbols[i], '+');
        if (firstparen != 0 && lastparen != 0 && firstparen < lastparen)
        {
            bt += ": ";
            *lastparen = '\0';
            char *realname = abi::__cxa_demangle(firstparen + 1, 0, 0, &status);
            if (realname != NULL)
                bt += realname;
            free(realname);
        }
#endif
        bt += EOL;
    }

    fprintf(file, "%s", bt.c_str());

    free(symbols);
}
#else // defined(UNIX) || defined(OSX)
void write_stack_trace(FILE* file, int ignore_count)
{
    const char* msg = "Unable to get stack trace on this platform." EOL;
    fprintf(stderr, "%s", msg);
    fprintf(file, "%s", msg);
}
#endif

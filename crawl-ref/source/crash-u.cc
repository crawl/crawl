/*
 *  File:       crash-u.cc
 *  Summary:    UNIX specific crash handling functions.
 *  Written by: Matthew Cline
 */

#include "AppHdr.h"

#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

#if defined(UNIX)
        #define BACKTRACE_SUPPORTED
#endif

#ifdef BACKTRACE_SUPPORTED
#if defined(TARGET_CPU_MIPS) || \
    defined(TARGET_OS_FREEBSD) || \
    defined(TARGET_OS_NETBSD) || \
    defined(TARGET_OS_OPENBSD) || \
    defined(TARGET_COMPILER_CYGWIN)
        #undef BACKTRACE_SUPPORTED
#endif
#endif

#ifdef BACKTRACE_SUPPORTED

#include <cxxabi.h>

#if !defined(TARGET_OS_MACOSX) && \
    !defined(TARGET_OS_WINDOWS) && \
    !defined(TARGET_COMPILER_CYGWIN)
#include <execinfo.h>
#endif

#ifdef TARGET_OS_MACOSX
#include <dlfcn.h>

typedef int (*backtrace_t)(void * *, int);
typedef char **(*backtrace_symbols_t)(void * const *, int);

// Used to convert from void* to function pointer (without a
// compiler warning).
template <typename TO, typename FROM> TO nasty_cast(FROM f)
{
    union
    {
        FROM f;
        TO t;
    } u;

    u.f = f;

    return u.t;
}
#endif // TARGET_OS_MACOSX

#endif // defined(UNIX) || defined(TARGET_OS_MACOSX)

#include "crash.h"
#include "dbg-crsh.h"

#include "externs.h"
#include "files.h"
#include "options.h"
#include "state.h"
#include "stuff.h"
#include "initfile.h"

/////////////////////////////////////////////////////////////////////////////
// Code for printing out debugging info on a crash.
////////////////////////////////////////////////////////////////////////////
#ifdef USE_UNIX_SIGNALS
static int _crash_signal    = 0;
static int _recursion_depth = 0;

static void _crash_signal_handler(int sig_num)
{
    if (crawl_state.game_crashed)
    {
        if (_recursion_depth > 0)
            return;
        _recursion_depth++;

        fprintf(stderr, "Recursive crash.\n");

        std::string dir = (!Options.morgue_dir.empty() ? Options.morgue_dir :
                           !SysEnv.crawl_dir.empty()   ? SysEnv.crawl_dir
                                                       : "");

        if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
            dir += FILE_SEPARATOR;

        char name[180];

        snprintf(name, sizeof(name), "%scrash-recursive-%s-%s.txt", dir.c_str(),
                you.your_name.c_str(), make_file_time(time(NULL)).c_str());

        FILE* file = fopen_replace(name);

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

    if (crawl_state.game_wants_emergency_save && crawl_state.need_save
        && !crawl_state.saving_game)
    {
        save_game(true, NULL);
    }

    // Now crash for real.
    signal(sig_num, SIG_DFL);
    raise(sig_num);
}
#endif

void init_crash_handler()
{
#if defined(USE_UNIX_SIGNALS)

    for (int i = 1; i <= 64; i++)
    {
        if (i == SIGALRM)
            continue;
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
#ifdef SIGTTOU
        if (i == SIGTTOU)
            continue;
#endif
#ifdef SIGTTIN
        if (i == SIGTTIN)
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
#if defined(UNIX)
    const char *name = strsignal(_crash_signal);
    if (name == NULL)
        name = "INVALID";

    fprintf(file, "Crash caused by signal #%d: %s\n\n", _crash_signal,
            name);
#endif
}

#if defined(BACKTRACE_SUPPORTED)
void write_stack_trace(FILE* file, int ignore_count)
{
    void* frames[50];

#if defined (TARGET_OS_MACOSX)
    backtrace_t backtrace;
    backtrace_symbols_t backtrace_symbols;
    backtrace = nasty_cast<backtrace_t, void*>(dlsym(RTLD_DEFAULT, "backtrace"));
    backtrace_symbols = nasty_cast<backtrace_symbols_t, void*>(dlsym(RTLD_DEFAULT, "backtrace_symbols"));
    if (!backtrace || !backtrace_symbols)
    {
        fprintf(stderr, "Couldn't get a stack trace.\n");
        fprintf(file, "Couldn't get a stack trace.\n");
        return;
    }
#endif

    int num_frames = backtrace(frames, ARRAYSZ(frames));
    char **symbols = backtrace_symbols(frames, num_frames);

#if !defined(TARGET_OS_MACOSX)
    if (symbols == NULL)
    {
        fprintf(stderr, "Out of memory.\n");
        fprintf(file,   "Out of memory.\n");

        // backtrace_symbols_fd() can print out the stack trace even if
        // malloc() can't find any free memory.
        backtrace_symbols_fd(frames, num_frames, fileno(file));
        return;
    }
#endif

    fprintf(file, "Obtained %d stack frames.\n", num_frames);

    // Now we prettify the printout to even show demangled C++ function names.
    std::string bt = "";
    for (int i = 0; i < num_frames; i++)
    {
#if defined (TARGET_OS_MACOSX)
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
#else // TARGET_OS_MACOSX
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
        bt += "\n";
    }

    fprintf(file, "%s", bt.c_str());

    free(symbols);
}
#else // defined(UNIX)
void write_stack_trace(FILE* file, int ignore_count)
{
    const char* msg = "Unable to get stack trace on this platform.\n";
    fprintf(stderr, "%s", msg);
    fprintf(file, "%s", msg);
}
#endif

/*
 *  File:       crash-u.cc
 *  Summary:    UNIX specific crash handling functions.
 *  Written by: Matthew Cline
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

#ifdef __GLIBC__
#include <execinfo.h>
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

#ifdef __GLIBC__
// NOTE: This should work on OS X, according to
// http://developer.apple.com/DOCUMENTATION/DARWIN/Reference/ManPages/man3/backtrace_symbols.3.html

void write_stack_trace(FILE* file, int ignore_count)
{
    void* frames[50];

    int num_frames = backtrace(frames, ARRAYSZ(frames));

    char **symbols = backtrace_symbols(frames, num_frames);

    if (symbols == NULL)
    {
        fprintf(stderr, "Out of memory." EOL);
        fprintf(file,   "Out of memory." EOL);

        // backtrace_symbols_fd() can print out the stack trace even if
        // malloc() can't find any free memory.
        backtrace_symbols_fd(frames, num_frames, fileno(file));
        return;
    }

    for (int i = ignore_count; i < num_frames; i++)
    {
        fprintf(file, "%s" EOL, symbols[i]);
    }

    free(symbols);
}
#else // ifdef __GLIBC__
void write_stack_trace(FILE* file, int ignore_count)
{
    const char* msg = "Unable to get stack trace on this platform." EOL;
    fprintf(stderr, msg);
    fprintf(file, msg);
}
#endif

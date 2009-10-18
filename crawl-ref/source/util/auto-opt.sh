#!/bin/bash --noprofile

#########################################################################
# Called by the makefile if AUTO_OPT_GCC is true, outputs the list of CPU
# specific optimization flags that should be used.
#########################################################################

#TODO: Detect extra compiler options for:
#
# * Non-gcc compilers.
# * CYGWIN.
# * Mac OS X.
# * Non-x86 Linux systems.

#########################################################################

# This line makes GCC automatically detect the CPU architecture to use.  Also
# might cause the use of some of the "-m" options we use down below,
# but GCC doesn't complain about an "-m" option being used repeatedly.
OUT="-mtune=native -march=native"

if [ -f /proc/cpuinfo ]; then
    # On Linux system, the "flags: " line of /proc/cpuinfo indicates which
    # extended instruction sets are available (among other things), so we
    # can use that to detect what to use.

    # Only get info for the first CPU; assumes that all CPUs in the system
    # are the same
    INFO=`cat /proc/cpuinfo | grep '^flags.*:' | head -n 1`

    # These are currently all x86 specific.
    for flag in cx16 mmx sse sse2 sse3 sse4.1 sse4.2 sse4 sse4a 3dnow abm; do
        # Put a space on either side of the flag name, so that it won't
        # match in the middle of a flag.
        if [[ $INFO == *\ $flag\ * ]] ; then
            OUT="$OUT -m$flag"
        fi
    done

    if [[ $INFO == *\ lahf_lm\ * ]]; then
        OUT="$OUT -msahf"
    fi

    # Any available SSE lets us use -mfpmath=sse
    if [[ $INFO == *\ sse* ]]; then
        OUT="$OUT -mfpmath=sse"
    fi
fi

echo "$OUT"

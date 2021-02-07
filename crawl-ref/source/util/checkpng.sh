#!/usr/bin/env bash

# Copyright © 2021 randomMesh@github.com
#
# This is a script to de-interlace, compress and fix all the PNG images of DSCC (https://github.com/crawl/crawl/)
# Hint: This script is able to convert images in any project, not just crawl
#
# It uses the OptiPNG command line tool which you need to install first
# To install it, download the source at http://optipng.sourceforge.net/ and compile it:
#
# Build instructions
# ------------------
#   On Unix, or under a Bourne-compatible shell, run ./configure and make:
#        cd optipng-0.7.7/
#        ./configure
#        make
#        make test
#
#  Alternatively, use a pre-configured makefile that matches your compiler;
#  e.g.:
#        cd optipng-0.7.7/
#        nmake -f build/visualc.mk
#        nmake -f build/visualc.mk test
#
# Before you run this script, you need to set the variables to point to the correct directories and files:
#
#	CRAWLDIR    [ the directory containing the crawl images ]
#	OPTIPNG     [ the path to the binary of OptiPNG ]
#	LOG_FILE    [ the log file to use if parameter -l is given ]
#
# The script has the following parameters which are optional:
#	-n [no change, only report]
#	-m [only check modified files in the git index] (not working yet)
#	-l [ write standard out to a logfile ]
#
# In order to run the script you need to set the executable bit
# chmod +x ./checkpng.sh
# once and the run it
#
# Examples:
#
#	./checkpng.sh -l     - Run the real thing, logged to LOG_FILE (recommended)
#	./checkpng.sh -n -l  - Run reports only, logged to LOG_FILE
#	./checkpng.sh -n     - Run reports only with thousands of lines flying by
#	./checkpng.sh        - Run the real thing with thousands of lines flying by
#
# Licence: The zlib/libpng License (Zlib)
#
# This software is provided 'as-is', without any express or implied warranty.
# In no event will the authors be held liable for any damages arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it freely,
# including subject to the following restrictions:
# 
#  1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
#     If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
#  2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
#  3. This notice may not be removed or altered from any source distribution.

set -o errexit -o pipefail -o noclobber -o nounset

# ---- CONFIG BEGIN ----
CRAWLDIR=~/development/crawl/crawl-ref/source/rltiles/
OPTIPNG=~/development/optipng-0.7.7/src/optipng/optipng
LOG_FILE=~/development/optipng.log
# ---- CONFIG END ----

# parse command line parameters
REPORTONLY=false
GITONLY=false
WRITELOG=false

while [[ "$#" -gt 0 ]]; do
    case $1 in
		-n|--report) REPORTONLY=true ;;
		-m|--git)    GITONLY=true  ;;
		-l|--log)    WRITELOG=true  ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

# The OptiPNG parameters used
OPTIPNGPARAMS="-fix -i=0 -o=7 -strip=all"

# manipulate parameters to OptiPNG based on command line parameters
if [[ "${REPORTONLY}" = true ]] ; then
	OPTIPNGPARAMS="${OPTIPNGPARAMS} -simulate"
fi

if [[ "${GITONLY}" = true ]] ; then
	: # TODO: implement only checking modified files in the git index
fi

# should we write the output to a log file?
if [[ "${WRITELOG}" = true ]] ; then

	# Open standard out at `$LOG_FILE` for write.
	exec 1>$LOG_FILE

	# Redirect standard error to standard out such that 
	# standard error ends up going to wherever standard
	# out goes (the file).
	exec 2>&1
fi

# and go...
for img in `find ${CRAWLDIR} -type f -name "*.png"`; do
	`${OPTIPNG} ${OPTIPNGPARAMS} ${img}`
done

exit 0


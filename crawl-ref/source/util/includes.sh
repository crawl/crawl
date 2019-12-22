# A bunch of shell functions that help finding unnecessary includes.
# Relies on appropriate versions of etags, grep, tr, awk and maybe bash.
#
# To use:
#   $ . util/includes.sh
#   $ checkall

# Tries to list all names defined in the header file $1.
# Indented names are assumed to be members that don't need to be checked,
# which will fail for indented declarations with #ifdefs.
_wrap_etags()
{
    tmp=$1.etags.h
    # clean up const member functions since they confuse etags
    sed -e 's/) const\;/)\;/' < $1 > $tmp
    etags --declarations $tmp -o -
    rm $tmp
}

names ()
{
    b=$(basename $1 .h);
    {
        _wrap_etags $1 \
            | grep '\(^[#a-z]\|^ *[0-9A-Z_][0-9A-Z_][0-9A-Z_]\)' \
            | grep -v '^'$b'\.h' \
            | tr '\177' '\t' | awk '{ print $(NF-1) }' \
        # etags misses namespace declarations
        grep namespace $1 | awk '{ print $NF }'
        # and some function definitions...
        grep '^[a-z]' $1 | grep '(' | cut -d'(' -f 1 | awk '{ print $NF }'
    } | sed -e 's/^\**//;s/[^a-zA-Z0-9]$//' | sort | uniq
}

# lists possible uses in $2 of names defined in $1
mightuse ()
{
    for n in $(names $1);
    do
        if grep -F $n $2; then
            return 0;
        fi;
    done;
    return 1
}

# checks whether source file $2 #include's $1
includes ()
{
    grep '#include "'$1'"' $2
}

# echo arguments if $2 includes $1 put shouldn't
check ()
{
    if includes $1 $2 > /dev/null && ! mightuse $1 $2 > /dev/null; then
        echo $1 $2;
    fi
}

# run check on all source for a given header
# should really cache the result of "names"
checkhdr ()
{
    for src in *.cc; do
        check $1 $src
    done
}

# Run check on all headers for a given source
# This is useful when you just want to check a single source file, most
# likely because that source file has recently been heavily modified.
checkcc ()
{
    for hdr in *.h; do
        check $hdr $1
    done
}

# run check on all pairs
checkall ()
{
    checkafter ""
}

# Run checkhdr on all pairs alphabetically after a specific header
# Useful to run directly in the case running checkall is interrupted.
checkafter ()
{
    for hdr in *.h; do
        if [[ $hdr < $1 || $hdr = AppHdr.h ]]; then
            continue;
        fi

        checkhdr $hdr
    done
}

# remove #include of $1 from $2
remove ()
{
    b=$(basename $1 .h)
    re='^#include "'$b'\.h"$'
    if grep "$re" $2 > /dev/null; then
        sed -e '/^#include "'$b'\.h"$/d' < $2 > $2.rmincl
        mv $2.rmincl $2
    fi
}

# remove doubtful includes for a list as output by checkall.
removelst ()
{
    while read hdr src; do
        remove $hdr $src
    done
}

# remove special includes
cleanup ()
{
    sed -e '/^quiver.h/d;
            /^ghost.h kills.cc$/d;
            /^cmd-name.h macro.cc$/d;
            /^cmd-keys.h macro.cc$/d;
            /^mon-spell.h monster.cc$/d;
            /^AppHdr.h /d;
            / artefact.cc/d'
}

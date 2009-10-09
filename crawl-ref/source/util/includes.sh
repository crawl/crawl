# A bunch of shell functions that help finding unnecessary includes.
# Relies on appropriate versions of etags, grep, tr, awk and maybe bash.
#
# To use:
#   $ . util/include.sh
#   $ checkall


# Tries to list all names defined in the header file $1.
# Indented names are assumed to be members that don't need to be checked,
# which will fail for indented declarations with #ifdefs.
names () 
{ 
    b=$(basename $1 .h);
    etags --declarations -D $1 -o - | grep '^[#a-z]' | grep -v '^'$b'\.h' \
        | tr '\177' '\t' | awk '{ print $(NF-1) }'
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

# run check on all pairs
checkall ()
{
    for hdr in *.h; do
        if [ $hdr = AppHdr.h ]; then
            continue;
        fi
        checkhdr $hdr
    done
}

# remove #include of $1 from $2
remove ()
{
    b=$(basename $1 .h)
    sed -e '/^#include "'$b'\.h"$/d' < $2 > $2.rmincl
    mv $2.rmincl $2
}

# remove doubtful includes for a list as output by checkall.
removelst ()
{
    while read hdr src; do
        remove $hdr $src
    done
}


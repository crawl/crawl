#!/usr/bin/env bash

# This is for testing the dialog facilities of Webtiles
# which are used for save transfer handling on the servers.

wcat() {
    tr '\n' ' ' | sed -e 's/ $//'
    echo
}


echo '{"msg":"layer", "layer":"crt"}'
echo -n '{"msg":"show_dialog", "html":"'

wcat <<EOF
<p>There's a newer version (foo) that can load your save.</p>
<p>[T]ransfer your save to this version?</p>
<input type='button' class='button' data-key='N' value='No' style='float:right;'>
<input type='button' class='button' data-key='T' value='Yes' style='float:right;'>
"}
EOF

read -n 1 -s REPLY

echo '{"msg":"hide_dialog"}'

if test "$REPLY" = "t" -o "$REPLY" = "T" -o "$REPLY" = "y"
then
    echo -n '{"msg":"show_dialog", "html":"'
    wcat <<EOF
<p>Transferring successful!</p>
<input type='button' class='button' data-key=' ' value='Continue' style='float:right;'>
"}
EOF
    read -n 1 -t 5 -s
    echo '{"msg":"hide_dialog"}'
fi

./crawl "$@"

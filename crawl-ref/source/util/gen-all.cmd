:: 1139-1141 in Makefile

:: docs/FAQ.html This file needs
:: to be run from inside /util,
:: or it can't find FAQ.txt
perl FAQ2html.pl

cd ..
:: art-enum.h ,art-data.h
perl util/art-data.pl
:: mon-mst.h
perl util/gen-mst.pl
:: cmd-name.h
perl util/cmd-name.pl
:: dat/dlua/tags.lua
perl util/gen-luatags.pl
:: mi-enum.h
perl util/gen-mi-enum
:: docs/aptitudes.txt
perl util/gen-apt.pl ../docs/aptitudes.txt ../docs/template/apt-tmpl.txt species-data.h aptitudes.h
:: docs/aptitudes-wide.txt
perl util/gen-apt.pl ../docs/aptitudes-wide.txt ../docs/template/apt-tmpl-wide.txt species-data.h aptitudes.h

::Change encoding to UTF-8
::which cuts the size in half
chcp 65001
:: crawl_manual.txt
perl util/unrest.pl ../docs/crawl_manual.rst > ../docs/crawl_manual.txt

cd util
pause

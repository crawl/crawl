@echo off
title DCSS Prebuild

set PATH=%MSYS_BIN%;%PATH%;



cd ..

echo Prebuilding headers...
perl.exe "util/art-data.pl" > NUL
perl.exe "util/gen-mst.pl"

perl.exe "util/gen_ver.pl" build.h
perl.exe "util/gen-cflg.pl" compflag.h "<UNKNOWN>" "<UNKNOWN>" "MSVC" "MSVC"



cd util

bison -y -d -b levcomp levcomp.ypp  > NUL

flex -olevcomp.lex.cc levcomp.lpp  > NUL

copy levcomp.lex.cc ..\prebuilt\levcomp.lex.cc  > NUL
copy levcomp.tab.c ..\prebuilt\levcomp.tab.cc  > NUL
copy levcomp.tab.h ..\prebuilt\levcomp.tab.h  > NUL

if "%1"=="TILES" goto do_png
goto skip_png
:do_png
cd ..\rltiles

for %%I in (main dngn floor wall feat player gui icons) do echo Generating %%I.png... & tool\tilegen.exe dc-%%I.txt

echo Copying png files...
copy *.png ..\dat\tiles\ > NUL

:skip_png

cd ..\contrib

echo Copying library headers...

xcopy sdl\include\*.h install\msvc\include\sdl\ /Y > NUL

copy sdl-image\SDL_image.h install\msvc\include\sdl\ > NUL

xcopy freetype\include install\msvc\include\freetype2 /i /s /Y > NUL

for %%I in (lauxlib.h lua.h luaconf.h lualib.h) do copy lua\src\%%I install\msvc\include\ > NUL

copy pcre\pcre.h install\msvc\include\ > NUL

for %%I in (zlib.h zconf.h) do copy zlib\%%I install\msvc\include\ > NUL

for %%I in (png.h pngconf.h) do copy libpng\%%I install\msvc\include\ > NUL

copy sqlite\sqlite3.h install\msvc\include\ > NUL
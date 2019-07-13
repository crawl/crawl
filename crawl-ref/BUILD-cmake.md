# Building crawl with CMake
Prefered method is an out of source build. Create a build directory outside of
your source tree, e.g.:
```
~/crawl        <--source
~/build-crawl  <--build
cd build-crawl
cmake -DCMAKE_BUILD_TYPE=Debug -DTILE_MODE=LOCAL_TILES ../crawl/cral-ref
make -j8
```

alternatively you can use ninja instead of make for slightly faster rebuilds:
```
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DTILE_MODE=LOCAL_TILES ../crawl/crawl-ref
ninja
```

NOTE: The path passed to the cmake command needs to poin to the folder containing the root CMakeLists.txt (which is located in crawl-ref)

## Packages requried to build
### Ubuntu 18.10
bison
flex
libsdl2-dev
libsdl2-image-dev
libfreetype6-dev
libpng-dev
libsqlite3-dev
libpng-dev
libluajit-5.1-dev
liblua5.1-0-dev

### MacOS (HomeBrew)
Note the packages should all be available with macports as well.
bison
flex
sqlite
lua@5.1
sdl2
sdl2_image
perl
freetype
libpng

## TODO-List
* Fixup compflag.h
* Enable tests (probably needs to be broken down)
* Add travis jobs that build with cmake & test linux variants
* Windwos (big, breakdown missing)

## Done
* ~~Add USE_MERGE_BASE~~
* ~~Update settings doc~~
* Fixup staging folder for webtiles
* Write some doc how to change current default player ini & config.py to run
  webtiles locally from staging folder (no changes necesary anymore)

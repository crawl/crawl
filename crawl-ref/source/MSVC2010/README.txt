prerequisites for VS2010 build:

1) DirectX SDK and DXSDK_DIR enviroment variable set (pointing to SDK root folder)
2) MSYS with following packages:
    a) msys-bison
    b) msys-flex
    c) msys-perl
    d) (optional) git
    e) MSYS_BIN enviroment variable set (pointing to msys' bin folder)

Building dependencies:
    a) open contrib\contrib-vs2010.sln
    b) choose solution configuration (either Release or Release LTCG, see below for details. However, you may build both - it doesn't take long)
    c) build solution

Building DCSS itself currently has two ways (due to issue with "set PATH" command when running batch file from inside VS2010):
- recomended:
    a) open msvc2010\crawl-ref.sln
    b) choose configuration (if you selected LTCG build for deps, you must select LTCG build now too. for all other builds including debug ones Release deps are needed)
    c) if you've chosen one of the "Tiles" configurations - build tilegen now
    d) go to msvc folder and run prebuild.bat or prebuild-tiles.bat
    e) build crawl

- not recomended (due to possible name clashes with native Windows tools):
    a) add msys' bin directory to PATH
    b) open msvc2010\crawl-ref.sln
    c) choose configuration (same as above)
    d) go to crawl project Properties -> Configuration Properties -> Build Events -> Pre-Build Event and change "Use In Build" to "Yes"
    d) build solution

Notes:
- LTCG = Link-Time Code Generation, advanced optimization that allows for better optimized executables at cost of build-time resource usage
- Resulting binaries will be *potentially* compatible with Win2000 (fixed missing EncodePointer/DecodePointer)
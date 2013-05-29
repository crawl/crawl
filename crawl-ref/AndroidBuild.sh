#!/bin/bash

########################################################################
# get all (ALL!) of the environment variables set properly
IFS='
'
# host arch
MYARCH=linux-x86
if uname -s | grep -i "linux" > /dev/null ; then
	MYARCH=linux-x86
fi
if uname -s | grep -i "darwin" > /dev/null ; then
	MYARCH=darwin-x86
fi
if uname -s | grep -i "windows" > /dev/null ; then
	MYARCH=windows-x86
fi

# where the NDK lives
NDK=`which ndk-build`
NDK=`dirname $NDK`

# are we using crystax?
#CRYSTAX=1

# GCC & toolchain
GCCPREFIX=arm-linux-androideabi
GCCVER=4.6.3
PLATFORMVER=android-14
TOOLCHAIN_DIR=$NDK/custom-toolchain-14
if [ ! -d "$TOOLCHAIN_DIR" ]; then
	echo "$NDK/build/tools/make-standalone-toolchain.sh --platform=$PLATFORMVER --install-dir=$TOOLCHAIN_DIR"
	$NDK/build/tools/make-standalone-toolchain.sh --platform=$PLATFORMVER --install-dir=$TOOLCHAIN_DIR
fi

SYSROOT=$TOOLCHAIN_DIR/sysroot

# where we are
LOCAL_PATH=`dirname $0`/..
LOCAL_PATH=`cd $LOCAL_PATH && pwd`

# all libraries that are available
APP_MODULES=`grep 'APP_MODULES [:][=]' $LOCAL_PATH/../Settings.mk | sed 's@.*[=]\(.*\)@\1@' | sed 's/sdl_main//;s/stdout-test//;s/application//;s/stlport//'`
# static libraries that are available
APP_AVAILABLE_STATIC_LIBS=`grep 'APP_AVAILABLE_STATIC_LIBS [:][=]' $LOCAL_PATH/../Settings.mk | sed 's@.*[=]\(.*\)@\1@'`
# shared libraries we're going to use
APP_SHARED_LIBS=$(
echo $APP_MODULES | xargs -n 1 echo | while read LIB ; do
	STATIC=`echo $APP_AVAILABLE_STATIC_LIBS application sdl_main stdout-test | grep "\\\\b$LIB\\\\b"`
	if [ -n "$STATIC" ] ; then true
	else
		echo $LIB
	fi
done
)
# static libraries we're going to use
APP_STATIC_LIBS=$(
echo $APP_MODULES | xargs -n 1 echo | while read LIB ; do
	STATIC=`echo $APP_AVAILABLE_STATIC_LIBS application sdl_main stdout-test | grep "\\\\b$LIB\\\\b"`
	if [ -n "$STATIC" ] ; then
		echo $LIB
	fi
done
)

# libraries that have a missing "include" directory (including some CrystaX integration)
MISSING_INCLUDE="-isystem$LOCAL_PATH/../sqlite  -isystem$NDK/sources/crystax/include"
MISSING_LIB=

# tell tools what the final shared library will be called
SHARED="-shared -Wl,-soname,libapplication.so"
if [ -n "$BUILD_EXECUTABLE" ]; then
	SHARED=
fi
if [ -n "$NO_SHARED_LIBS" ]; then
	APP_SHARED_LIBS=
fi

# CFLAGS
CFLAGS="\
-fexceptions -frtti \
-fpic -ffunction-sections -funwind-tables -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__  -Wno-psabi \
-march=armv5te -mtune=xscale -msoft-float -mthumb -Os -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64 \
-isystem$SYSROOT/usr/include -Wa,--noexecstack \
-DANDROID \
-DNDEBUG -O2 -g \
-isystem$NDK/sources/cxx-stl/gnu-libstdc++/include \
-isystem$NDK/sources/cxx-stl/gnu-libstdc++/libs/armeabi/include \
-isystem$LOCAL_PATH/../sdl-1.2/include \
`echo $APP_MODULES | sed \"s@\([-a-zA-Z0-9_.]\+\)@-isystem$LOCAL_PATH/../\1/include@g\"` \
$MISSING_INCLUDE"

# LDFLAGS
LDFLAGS="\
-fexceptions -frtti $SHARED \
--sysroot=$NDK/platforms/$PLATFORMVER/arch-arm \
-Wl,-rpath-link=$SYSROOT/usr/lib \
-Wl,--no-undefined -Wl,-z,noexecstack \
"

# LIBS
LIBS="\
`[ ! -z \"$CRYSTAX\"  ] && echo '-lcrystax_static'` \
`echo $APP_SHARED_LIBS | sed \"s@\([-a-zA-Z0-9_.]\+\)@$LOCAL_PATH/../../obj/local/armeabi/lib\1.so@g\"` \
`echo $APP_STATIC_LIBS | sed 's/\([-a-zA-Z0-9_.]*\)/-l\1/g'` \
$SYSROOT/usr/lib/libc.so \
$SYSROOT/usr/lib/libm.so \
$SYSROOT/usr/lib/libGLESv1_CM.so \
$SYSROOT/usr/lib/libdl.so \
$SYSROOT/usr/lib/liblog.so \
$SYSROOT/usr/lib/libz.so \
-L$NDK/sources/cxx-stl/gnu-libstdc++/libs/armeabi/4.6.3 \
-L$LOCAL_PATH/../../obj/local/armeabi \
$MISSING_LIB"

# set environment for a command
function setEnv {
env PATH=$TOOLCHAIN_DIR/bin:$LOCAL_PATH:$PATH \
CFLAGS="$CFLAGS" \
CXXFLAGS="$CFLAGS" \
LDFLAGS="$LDFLAGS" \
LIBS="$LIBS" \
CC="$TOOLCHAIN_DIR/bin/$GCCPREFIX-gcc" \
CXX="$TOOLCHAIN_DIR/bin/$GCCPREFIX-g++" \
RANLIB="$TOOLCHAIN_DIR/bin/$GCCPREFIX-ranlib" \
LD="$TOOLCHAIN_DIR/bin/$GCCPREFIX-g++" \
AR="$TOOLCHAIN_DIR/bin/$GCCPREFIX-ar" \
CPP="$TOOLCHAIN_DIR/bin/$GCCPREFIX-cpp $CFLAGS" \
NM="$TOOLCHAIN_DIR/bin/$GCCPREFIX-nm" \
AS="$TOOLCHAIN_DIR/bin/$GCCPREFIX-as" \
STRIP="$TOOLCHAIN_DIR/bin/$GCCPREFIX-strip" \
"$@"
}

########################################################################
# actually do the compile(!)

CRAWL_PATH=`dirname $0`/source
CRAWL_PATH=`cd $CRAWL_PATH && pwd`

# remove the final artefact
rm -f libapplication.so

# these are probably right
PREFIX_P="/sdcard/Android/data/org.develz.crawl/files"
DATADIR_P="."
SAVES_P="."
GLES=1

# "make install"
cd $CRAWL_PATH && setEnv nice make -j2 install TILES=1 TOUCH_UI=1 CROSSHOST=arm-linux-androideabi ANDROID=1  prefix=$PREFIX_P DATADIR=$DATADIR_P SAVEDIR=$SAVES_P GLES=$GLES COPY_FONTS=1
cd $CRAWL_PATH/..

if [ -e bin/crawl ]; then
	# crawl compiles to bin/crawl - put the lib in the right place
	cp -f bin/crawl libapplication.so

	# compile all the tiles data into a zip archive for the .apk
	cd AndroidData && rm -f crawl-data.zip* && zip -q -r crawl-data.zip * && cd -

	# put all the android icons in the right places to be picked up
	for RES in ldpi mdpi hdpi xhdpi; do
		for ICON in icon; do
			if [ -f source/dat/tiles/android/$ICON-$RES.png ]; then
				mkdir -p ../../../res/drawable-$RES
				cp source/dat/tiles/android/$ICON-$RES.png ../../../res/drawable-$RES/$ICON.png
			fi
		done
	done

	# symlink a default icon
	ln -s -f source/dat/tiles/stone_soup_icon-512x512.png icon.png

  # fixup version string
  VER=$(sed '/LONG/!d; s/^.*"\(.*\)"$/\1/' < source/build.h)
  echo `pwd`
  sed -i "/android:versionName/s/GEN_FROM_HEADER/$VER/" $LOCAL_PATH/../../AndroidManifest.xml
fi


# Uncomment this if you're using STL in your project
# See CPLUSPLUS-SUPPORT.html in the NDK documentation for more information
APP_STL := c++_shared

APP_ABI := armeabi armeabi-v7a x86

APP_CFLAGS := -fexceptions
APP_CXXFLAGS := -frtti $(APP_CFLAGS)

# Uncomment this if you're using STL in your project
# See CPLUSPLUS-SUPPORT.html in the NDK documentation for more information
APP_STL := c++_shared

APP_ABI := all

APP_CFLAGS := -fexceptions
APP_CXXFLAGS := -frtti -std=c++11 $(APP_CFLAGS)

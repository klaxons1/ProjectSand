cmake_minimum_required(VERSION 3.22.1)
project(sand)

add_library(sand SHARED
    sand_engine.cpp
    ported_decompile.cpp
)

find_library(log-lib log)
find_library(android-lib android)
find_library(native-window-lib nativewindow)

target_link_libraries(sand
    ${log-lib}
    ${android-lib}
    -landroid
)

target_include_directories(sand PRIVATE
    ${ANDROID_NDK}/sources/third_party/googletest/include
)

set_target_properties(sand PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Ensure all ABIs produce same soname as original libsand.so
set_target_properties(sand PROPERTIES
    OUTPUT_NAME sand
    PREFIX "lib"
    SUFFIX ".so"
)

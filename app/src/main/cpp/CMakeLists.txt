cmake_minimum_required(VERSION 3.22.1)
project(sand)

add_library(sand SHARED
    arm64/libsand_decompiled.cpp
)

if(ANDROID AND NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "The reconstructed engine currently targets arm64-v8a only")
endif()

find_library(log-lib log REQUIRED)
find_library(android-lib android REQUIRED)

target_link_libraries(sand PRIVATE
    ${log-lib}
    ${android-lib}
)

target_include_directories(sand PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

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

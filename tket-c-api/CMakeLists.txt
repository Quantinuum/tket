cmake_minimum_required(VERSION 3.23)
project(tket-c-api CXX)

# TKET dependency
find_package(tket REQUIRED)

# Recursive TKET dependencies. Not required when using conan, but needed to
# build directly from cmake
find_package(Boost CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(gmp CONFIG)
if (NOT gmp_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(gmp REQUIRED IMPORTED_TARGET gmp)
endif()
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SymEngine CONFIG REQUIRED)
find_package(tkassert CONFIG REQUIRED)
find_package(tklog CONFIG REQUIRED)
find_package(tkrng CONFIG REQUIRED)
find_package(tktokenswap CONFIG REQUIRED)
find_package(tkwsm CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS yes)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(Boost_NO_BOOST_CMAKE ON)

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /WX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wunreachable-code -Wunused")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Work around warnings from symengine about ciso646.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cpp")
endif()

if (NOT TARGET symengine::symengine)
    add_library(symengine::symengine ALIAS symengine)
endif()
if (NOT TARGET gmp::gmp)
    add_library(gmp::gmp ALIAS PkgConfig::gmp)
endif()

# Library and headers to be installed
add_library(tket-c-api src/tket-c-api.cpp)
target_include_directories(tket-c-api PUBLIC include)

target_link_libraries(tket-c-api PRIVATE tket::tket Eigen3::Eigen)

set_target_properties(tket-c-api PROPERTIES PUBLIC_HEADER "include/tket-c-api.h")
install(TARGETS tket-c-api)

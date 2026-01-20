cmake_minimum_required(VERSION 3.15)
project(PackageTest CXX)

find_package(tket-c-api CONFIG REQUIRED)
find_package(Threads)

add_executable(example src/example.cpp)
target_link_libraries(example tket-c-api::tket-c-api ${CMAKE_THREAD_LIBS_INIT})

cmake_minimum_required(VERSION 3.23)
project(test-tket-c CXX)

find_package(Catch2 CONFIG REQUIRED)

add_executable(test-tket-c
  src/basic.cpp
  )

target_link_libraries(test-tket-c PRIVATE tket_c)
target_link_libraries(test-tket-c PRIVATE Catch2::Catch2WithMain)
set(WITH_COVERAGE no CACHE BOOL "Link library with profiling for test coverage")
IF (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    IF (WITH_COVERAGE)
        target_link_options(test-tket-c PUBLIC --coverage)
    ENDIF()
ENDIF()

install(TARGETS test-tket-c DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )

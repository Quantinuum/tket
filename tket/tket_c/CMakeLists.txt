add_library(tket_c)
target_sources(tket_c
    PRIVATE
        src/tket.cpp
    PUBLIC FILE_SET HEADERS
      BASE_DIRS ${PROJECT_SOURCE_DIR}/include
      FILES ${PROJECT_SOURCE_DIR}/include/tket_c/tket.h
)
target_include_directories(tket_c PUBLIC
    $<INSTALL_INTERFACE:include/tket_c>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tket_c>)
target_link_libraries(tket_c PRIVATE tket)
target_link_libraries(tket_c PRIVATE Boost::headers)
target_link_libraries(tket_c PRIVATE Eigen3::Eigen)
target_link_libraries(tket_c PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(tket_c PRIVATE symengine::symengine)
target_link_libraries(tket_c PRIVATE tkassert::tkassert)
target_link_libraries(tket_c PRIVATE tklog::tklog)
target_link_libraries(tket_c PRIVATE tkrng::tkrng)
target_link_libraries(tket_c PRIVATE tktokenswap::tktokenswap)
target_link_libraries(tket_c PRIVATE tkwsm::tkwsm)
IF(APPLE)
    target_link_libraries(tket_c PRIVATE "-flat_namespace")
ENDIF()

if (BUILD_TKET_C_TEST)
    add_subdirectory(test)
endif()

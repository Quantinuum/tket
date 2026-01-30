# Copyright Quantinuum
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.23)
project(pytket CXX)

find_package(Boost CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(gmp CONFIG)
if (NOT gmp_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(gmp REQUIRED IMPORTED_TARGET gmp)
endif()
find_package(Python 3.8
  REQUIRED COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)
find_package(nanobind CONFIG REQUIRED)
find_package(SymEngine CONFIG REQUIRED)
find_package(tkassert CONFIG REQUIRED)
find_package(tkassert CONFIG REQUIRED)
find_package(tket CONFIG REQUIRED)
find_package(tklog CONFIG REQUIRED)
find_package(tkrng CONFIG REQUIRED)
find_package(tktokenswap CONFIG REQUIRED)
find_package(tkwsm CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(Boost_NO_BOOST_CMAKE ON)

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /WX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wunused")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
    # remove -Wno-deprecated-declarations once https://github.com/boostorg/boost/issues/688 is resolved
endif()

if (UNIX)
    # Allow binder libraries to load other shared libraries from same directory.
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "\${ORIGIN}")
endif()

if (NOT TARGET gmp::gmp)
    add_library(gmp::gmp ALIAS PkgConfig::gmp)
endif()
if (NOT TARGET symengine::symengine)
    add_library(symengine::symengine ALIAS symengine)
endif()

list(APPEND lib_deps
    Eigen3::Eigen
    gmp::gmp
    symengine::symengine
    tkassert::tkassert
    tket::tket
    tklog::tklog
    )
if (WIN32)
    list(APPEND lib_deps bcrypt) # For boost::uuid
endif()
if (APPLE)
    list(APPEND lib_deps "-flat_namespace")
endif()

set(HEADER_FILES
        binders/include/add_gate.hpp
        binders/include/binder_utils.hpp
        binders/include/circuit_registers.hpp
        binders/include/deleted_hash.hpp
        binders/include/nanobind_json/nanobind_json.hpp
        binders/include/nanobind-stl.hpp
        binders/include/py_operators.hpp
        binders/include/typecast.hpp
        binders/include/unit_downcast.hpp
        binders/include/UnitRegister.hpp
   )

nanobind_add_module(circuit
    STABLE_ABI
    NB_DOMAIN pytket
    binders/circuit/boxes.cpp
    binders/circuit/Circuit/add_classical_op.cpp
    binders/circuit/Circuit/add_op.cpp
    binders/circuit/Circuit/main.cpp
    binders/circuit/classical.cpp
    binders/circuit/clexpr.cpp
    binders/circuit/main.cpp
    ${HEADER_FILES})
target_include_directories(circuit PRIVATE binders/include)
target_link_libraries(circuit PRIVATE ${lib_deps})

nanobind_add_module(circuit_library
    STABLE_ABI
    NB_DOMAIN pytket
    binders/circuit_library.cpp ${HEADER_FILES})
target_include_directories(circuit_library PRIVATE binders/include)
target_link_libraries(circuit_library PRIVATE ${lib_deps})

nanobind_add_module(unit_id
    STABLE_ABI
    NB_DOMAIN pytket
    binders/unitid.cpp ${HEADER_FILES})
target_include_directories(unit_id PRIVATE binders/include)
target_link_libraries(unit_id PRIVATE ${lib_deps})

nanobind_add_module(mapping
    STABLE_ABI
    NB_DOMAIN pytket
    binders/mapping.cpp ${HEADER_FILES})
target_include_directories(mapping PRIVATE binders/include)
target_link_libraries(mapping PRIVATE ${lib_deps})

nanobind_add_module(transform
    STABLE_ABI
    NB_DOMAIN pytket
    binders/transform.cpp ${HEADER_FILES})
target_include_directories(transform PRIVATE binders/include)
target_link_libraries(transform PRIVATE ${lib_deps})

nanobind_add_module(predicates
    STABLE_ABI
    NB_DOMAIN pytket
    binders/predicates.cpp ${HEADER_FILES})
target_include_directories(predicates PRIVATE binders/include)
target_link_libraries(predicates PRIVATE ${lib_deps})

nanobind_add_module(passes
    STABLE_ABI
    NB_DOMAIN pytket
    binders/passes.cpp ${HEADER_FILES})
target_include_directories(passes PRIVATE binders/include)
target_link_libraries(passes PRIVATE ${lib_deps})

nanobind_add_module(architecture
    STABLE_ABI
    NB_DOMAIN pytket
    binders/architecture.cpp ${HEADER_FILES})
target_include_directories(architecture PRIVATE binders/include)
target_link_libraries(architecture PRIVATE ${lib_deps})

nanobind_add_module(placement
    STABLE_ABI
    NB_DOMAIN pytket
    binders/placement.cpp ${HEADER_FILES})
target_include_directories(placement PRIVATE binders/include)
target_link_libraries(placement PRIVATE ${lib_deps})

nanobind_add_module(partition
    STABLE_ABI
    NB_DOMAIN pytket
    binders/partition.cpp ${HEADER_FILES})
target_include_directories(partition PRIVATE binders/include)
target_link_libraries(partition PRIVATE ${lib_deps})

nanobind_add_module(pauli
    STABLE_ABI
    NB_DOMAIN pytket
    binders/pauli.cpp ${HEADER_FILES})
target_include_directories(pauli PRIVATE binders/include)
target_link_libraries(pauli PRIVATE ${lib_deps})

nanobind_add_module(logging
    STABLE_ABI
    NB_DOMAIN pytket
    binders/logging.cpp ${HEADER_FILES})
target_include_directories(logging PRIVATE binders/include)
target_link_libraries(logging PRIVATE ${lib_deps})

nanobind_add_module(utils_serialization
    STABLE_ABI
    NB_DOMAIN pytket
    binders/utils_serialization.cpp ${HEADER_FILES})
target_include_directories(utils_serialization PRIVATE binders/include)
target_link_libraries(utils_serialization PRIVATE ${lib_deps})

nanobind_add_module(tailoring
    STABLE_ABI
    NB_DOMAIN pytket
    binders/tailoring.cpp ${HEADER_FILES})
target_include_directories(tailoring PRIVATE binders/include)
target_link_libraries(tailoring PRIVATE ${lib_deps})

nanobind_add_module(tableau
    STABLE_ABI
    NB_DOMAIN pytket
    binders/tableau.cpp ${HEADER_FILES})
target_include_directories(tableau PRIVATE binders/include)
target_link_libraries(tableau PRIVATE ${lib_deps})

nanobind_add_module(zx
    STABLE_ABI
    NB_DOMAIN pytket
    binders/zx/diagram.cpp
    binders/zx/rewrite.cpp
    ${HEADER_FILES})
target_include_directories(zx PRIVATE binders/include)
target_link_libraries(zx PRIVATE ${lib_deps})

install(TARGETS
    architecture
    circuit
    circuit_library
    logging
    mapping
    partition
    passes
    pauli
    placement
    predicates
    tableau
    tailoring
    transform
    unit_id
    utils_serialization
    zx
    LIBRARY DESTINATION lib)

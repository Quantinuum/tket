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
project(tket CXX)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

find_package(Boost CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SymEngine CONFIG REQUIRED)
find_package(tkassert CONFIG REQUIRED)
find_package(tklog CONFIG REQUIRED)
find_package(tkrng CONFIG REQUIRED)
find_package(tktokenswap CONFIG REQUIRED)
find_package(tkwsm CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

option(INSTALL_NAME_DIR "Set the install name dir for the library to @loader_path for Apple targets" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()


# ccache stuff
if (CMAKE_GENERATOR MATCHES "Visual Studio" AND DEFINED ENV{TKET_VSGEN_CCACHE_EXE})
    file(COPY_FILE $ENV{TKET_VSGEN_CCACHE_EXE} ${CMAKE_BINARY_DIR}/cl.exe ONLY_IF_DIFFERENT)
    message("-> Setting ccache ($ENV{TKET_VSGEN_CCACHE_EXE}) as compiler launcher for generator ${CMAKE_GENERATOR}")

    # By default Visual Studio generators will use /Zi which is not compatible
    # with ccache, so tell Visual Studio to use /Z7 instead.
    message(STATUS "   Setting MSVC debug information format to 'Embedded'")
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

    set(CMAKE_VS_GLOBALS
        "CLToolExe=cl.exe"
        "CLToolPath=${CMAKE_BINARY_DIR}"
        "TrackFileAccess=false"
        "UseMultiToolTask=true"
        "DebugInformationFormat=OldStyle"
    )
else()
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
        message("-> Setting ccache (${CCACHE_PROGRAM}) as compiler launcher for generator ${CMAKE_GENERATOR}")
    endif ()
endif()

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS yes)
elseif(APPLE AND INSTALL_NAME_DIR)
    # set correct install_name
    set(CMAKE_INSTALL_NAME_DIR "@loader_path")
    set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

IF (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /EHsc")
ELSE()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wunreachable-code -Wunused")
ENDIF()
if(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Winconsistent-missing-override -Wloop-analysis")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Work around gcc bug related to std::optional, and warning from Eigen.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=maybe-uninitialized -Wno-error=stringop-overflow")
endif()


add_library(tket)

set(PROFILE_COVERAGE no CACHE BOOL "Build library with profiling for test coverage")
IF (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    IF (PROFILE_COVERAGE)
        target_compile_options(tket PRIVATE -g --coverage)
        target_link_options(tket PUBLIC --coverage)
    ENDIF()
ENDIF()

if (NOT TARGET symengine::symengine)
    add_library(symengine::symengine ALIAS symengine)
endif()

target_include_directories(tket PUBLIC
    $<INSTALL_INTERFACE:include/tket>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tket>)
target_link_libraries(tket PRIVATE Boost::headers)
target_link_libraries(tket PRIVATE Eigen3::Eigen)
target_link_libraries(tket PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(tket PRIVATE symengine::symengine)
target_link_libraries(tket PRIVATE tkassert::tkassert)
target_link_libraries(tket PRIVATE tklog::tklog)
target_link_libraries(tket PRIVATE tkrng::tkrng)
target_link_libraries(tket PRIVATE tktokenswap::tktokenswap)
target_link_libraries(tket PRIVATE tkwsm::tkwsm)
IF(APPLE)
    target_link_libraries(tket PRIVATE "-flat_namespace")
ENDIF()

target_sources(tket
    PRIVATE
        src/ArchAwareSynth/Path.cpp
        src/ArchAwareSynth/SteinerForest.cpp
        src/ArchAwareSynth/SteinerTree.cpp
        src/Architecture/Architecture.cpp
        src/Architecture/ArchitectureGraphClasses.cpp
        src/Architecture/ArchitectureMapping.cpp
        src/Architecture/BestTsaWithArch.cpp
        src/Architecture/DistancesFromArchitecture.cpp
        src/Architecture/NeighboursFromArchitecture.cpp
        src/Architecture/SubgraphMonomorphisms.cpp
        src/Characterisation/Cycles.cpp
        src/Characterisation/DeviceCharacterisation.cpp
        src/Characterisation/FrameRandomisation.cpp
        src/Circuit/AssertionSynthesis.cpp
        src/Circuit/basic_circ_manip.cpp
        src/Circuit/Boxes.cpp
        src/Circuit/CircPool.cpp
        src/Circuit/Circuit.cpp
        src/Circuit/CircuitJson.cpp
        src/Circuit/CircUtils.cpp
        src/Circuit/CommandJson.cpp
        src/Circuit/Conditional.cpp
        src/Circuit/ConjugationBox.cpp
        src/Circuit/ControlledGates.cpp
        src/Circuit/DAGProperties.cpp
        src/Circuit/DiagonalBox.cpp
        src/Circuit/DummyBox.cpp
        src/Circuit/latex_drawing.cpp
        src/Circuit/macro_circ_info.cpp
        src/Circuit/macro_manipulation.cpp
        src/Circuit/Multiplexor.cpp
        src/Circuit/OpJson.cpp
        src/Circuit/PauliExpBoxes.cpp
        src/Circuit/ResourceData.cpp
        src/Circuit/setters_and_getters.cpp
        src/Circuit/Simulation/BitOperations.cpp
        src/Circuit/Simulation/CircuitSimulator.cpp
        src/Circuit/Simulation/DecomposeCircuit.cpp
        src/Circuit/Simulation/GateNode.cpp
        src/Circuit/Simulation/GateNodesBuffer.cpp
        src/Circuit/Simulation/PauliExpBoxUnitaryCalculator.cpp
        src/Circuit/Slices.cpp
        src/Circuit/StatePreparation.cpp
        src/Circuit/SubcircuitFinder.cpp
        src/Circuit/ThreeQubitConversion.cpp
        src/Circuit/ToffoliBox.cpp
        src/Clifford/ChoiMixTableau.cpp
        src/Clifford/SymplecticTableau.cpp
        src/Clifford/UnitaryTableau.cpp
        src/Converters/ChoiMixTableauConverters.cpp
        src/Converters/Gauss.cpp
        src/Converters/PauliGraphConverters.cpp
        src/Converters/PhasePoly.cpp
        src/Converters/UnitaryTableauBox.cpp
        src/Converters/UnitaryTableauConverters.cpp
        src/Converters/ZXConverters.cpp
        src/Diagonalisation/Diagonalisation.cpp
        src/Diagonalisation/DiagUtils.cpp
        src/Diagonalisation/PauliPartition.cpp
        src/Gate/Gate.cpp
        src/Gate/GatePtr.cpp
        src/Gate/GateUnitaryMatrix.cpp
        src/Gate/GateUnitaryMatrixComposites.cpp
        src/Gate/GateUnitaryMatrixError.cpp
        src/Gate/GateUnitaryMatrixFixedMatrices.cpp
        src/Gate/GateUnitaryMatrixPrimitives.cpp
        src/Gate/GateUnitaryMatrixUtils.cpp
        src/Gate/GateUnitaryMatrixVariableQubits.cpp
        src/Gate/GateUnitarySparseMatrix.cpp
        src/Gate/OpPtrFunctions.cpp
        src/Gate/Rotation.cpp
        src/Gate/SymTable.cpp
        src/Graphs/AdjacencyData.cpp
        src/Graphs/ArticulationPoints.cpp
        src/Graphs/BruteForceColouring.cpp
        src/Graphs/ColouringPriority.cpp
        src/Graphs/GraphColouring.cpp
        src/Graphs/GraphRoutines.cpp
        src/Graphs/LargeCliquesResult.cpp
        src/Mapping/AASLabelling.cpp
        src/Mapping/AASRoute.cpp
        src/Mapping/BoxDecomposition.cpp
        src/Mapping/LexicographicalComparison.cpp
        src/Mapping/LexiLabelling.cpp
        src/Mapping/LexiRoute.cpp
        src/Mapping/LexiRouteRoutingMethod.cpp
        src/Mapping/MappingFrontier.cpp
        src/Mapping/MappingManager.cpp
        src/Mapping/MultiGateReorder.cpp
        src/Mapping/RoutingMethodCircuit.cpp
        src/Mapping/RoutingMethodJson.cpp
        src/Mapping/Verification.cpp
        src/MeasurementSetup/MeasurementReduction.cpp
        src/MeasurementSetup/MeasurementSetup.cpp
        src/Ops/BarrierOp.cpp
        src/Ops/ClassicalOps.cpp
        src/Ops/ClExpr.cpp
        src/Ops/FlowOp.cpp
        src/Ops/MetaOp.cpp
        src/Ops/Op.cpp
        src/Ops/OpJsonFactory.cpp
        src/OpType/OpDesc.cpp
        src/OpType/OpTypeFunctions.cpp
        src/OpType/OpTypeInfo.cpp
        src/OpType/OpTypeJson.cpp
        src/PauliGraph/ConjugatePauliFunctions.cpp
        src/PauliGraph/PauliGraph.cpp
        src/Placement/Frontier.cpp
        src/Placement/GraphPlacement.cpp
        src/Placement/LinePlacement.cpp
        src/Placement/MonomorphismCalculation.cpp
        src/Placement/NeighbourPlacements.cpp
        src/Placement/NoiseAwarePlacement.cpp
        src/Placement/Placement.cpp
        src/Placement/PlacementGraphClasses.cpp
        src/Predicates/CompilationUnit.cpp
        src/Predicates/CompilerPass.cpp
        src/Predicates/PassGenerators.cpp
        src/Predicates/PassLibrary.cpp
        src/Predicates/Predicates.cpp
        src/Transformations/BasicOptimisation.cpp
        src/Transformations/CliffordOptimisation.cpp
        src/Transformations/CliffordReductionPass.cpp
        src/Transformations/CliffordResynthesis.cpp
        src/Transformations/Combinator.cpp
        src/Transformations/ContextualReduction.cpp
        src/Transformations/Decomposition.cpp
        src/Transformations/GreedyPauliOptimisation.cpp
        src/Transformations/GreedyPauliConverters.cpp
        src/Transformations/GreedyPauliOps.cpp
        src/Transformations/MeasurePass.cpp
        src/Transformations/OptimisationPass.cpp
        src/Transformations/PauliOptimisation.cpp
        src/Transformations/PhaseOptimisation.cpp
        src/Transformations/PQPSquash.cpp
        src/Transformations/Rebase.cpp
        src/Transformations/RedundancyRemoval.cpp
        src/Transformations/Replacement.cpp
        src/Transformations/RzPhasedXSquash.cpp
        src/Transformations/SingleQubitSquash.cpp
        src/Transformations/StandardSquash.cpp
        src/Transformations/ThreeQubitSquash.cpp
        src/Utils/CosSinDecomposition.cpp
        src/Utils/Expression.cpp
        src/Utils/HelperFunctions.cpp
        src/Utils/MatrixAnalysis.cpp
        src/Utils/PauliTensor.cpp
        src/Utils/UnitID.cpp
        src/ZX/Flow.cpp
        src/ZX/MBQCRewrites.cpp
        src/ZX/ZXDConstructors.cpp
        src/ZX/ZXDExpansions.cpp
        src/ZX/ZXDFormats.cpp
        src/ZX/ZXDGettersSetters.cpp
        src/ZX/ZXDManipulation.cpp
        src/ZX/ZXDSubdiagram.cpp
        src/ZX/ZXGenerator.cpp
        src/ZX/ZXRWAxioms.cpp
        src/ZX/ZXRWCombinators.cpp
        src/ZX/ZXRWDecompositions.cpp
        src/ZX/ZXRWGraphLikeForm.cpp
        src/ZX/ZXRWGraphLikeSimplification.cpp
        src/ZX/ZXRWSequences.cpp
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
        include/tket/ArchAwareSynth/Path.hpp
        include/tket/ArchAwareSynth/SteinerForest.hpp
        include/tket/ArchAwareSynth/SteinerTree.hpp
        include/tket/Architecture/Architecture.hpp
        include/tket/Architecture/ArchitectureMapping.hpp
        include/tket/Architecture/BestTsaWithArch.hpp
        include/tket/Architecture/DistancesFromArchitecture.hpp
        include/tket/Architecture/NeighboursFromArchitecture.hpp
        include/tket/Architecture/SubgraphMonomorphisms.hpp
        include/tket/Characterisation/Cycles.hpp
        include/tket/Characterisation/DeviceCharacterisation.hpp
        include/tket/Characterisation/ErrorTypes.hpp
        include/tket/Characterisation/FrameRandomisation.hpp
        include/tket/Circuit/AssertionSynthesis.hpp
        include/tket/Circuit/Boxes.hpp
        include/tket/Circuit/CircPool.hpp
        include/tket/Circuit/Circuit.hpp
        include/tket/Circuit/CircUtils.hpp
        include/tket/Circuit/Command.hpp
        include/tket/Circuit/Conditional.hpp
        include/tket/Circuit/ConjugationBox.hpp
        include/tket/Circuit/DAGDefs.hpp
        include/tket/Circuit/DiagonalBox.hpp
        include/tket/Circuit/DummyBox.hpp
        include/tket/Circuit/Multiplexor.hpp
        include/tket/Circuit/PauliExpBoxes.hpp
        include/tket/Circuit/ResourceData.hpp
        include/tket/Circuit/Simulation/CircuitSimulator.hpp
        include/tket/Circuit/Simulation/PauliExpBoxUnitaryCalculator.hpp
        include/tket/Circuit/Slices.hpp
        include/tket/Circuit/StatePreparation.hpp
        include/tket/Circuit/ThreeQubitConversion.hpp
        include/tket/Circuit/ToffoliBox.hpp
        include/tket/Clifford/ChoiMixTableau.hpp
        include/tket/Clifford/SymplecticTableau.hpp
        include/tket/Clifford/UnitaryTableau.hpp
        include/tket/Converters/Converters.hpp
        include/tket/Converters/Gauss.hpp
        include/tket/Converters/PhasePoly.hpp
        include/tket/Converters/UnitaryTableauBox.hpp
        include/tket/Diagonalisation/Diagonalisation.hpp
        include/tket/Diagonalisation/DiagUtils.hpp
        include/tket/Diagonalisation/PauliPartition.hpp
        include/tket/Gate/Gate.hpp
        include/tket/Gate/GatePtr.hpp
        include/tket/Gate/GateUnitaryMatrix.hpp
        include/tket/Gate/GateUnitaryMatrixError.hpp
        include/tket/Gate/GateUnitaryMatrixImplementations.hpp
        include/tket/Gate/GateUnitaryMatrixUtils.hpp
        include/tket/Gate/OpPtrFunctions.hpp
        include/tket/Gate/Rotation.hpp
        include/tket/Gate/SymTable.hpp
        include/tket/Graphs/AbstractGraph.hpp
        include/tket/Graphs/AdjacencyData.hpp
        include/tket/Graphs/ArticulationPoints.hpp
        include/tket/Graphs/ArticulationPoints_impl.hpp
        include/tket/Graphs/CompleteGraph.hpp
        include/tket/Graphs/DirectedGraph.hpp
        include/tket/Graphs/GraphColouring.hpp
        include/tket/Graphs/GraphRoutines.hpp
        include/tket/Graphs/LargeCliquesResult.hpp
        include/tket/Graphs/TreeSearch.hpp
        include/tket/Graphs/TreeSearch_impl.hpp
        include/tket/Graphs/Utils.hpp
        include/tket/Graphs/Utils_impl.hpp
        include/tket/Mapping/AASLabelling.hpp
        include/tket/Mapping/AASRoute.hpp
        include/tket/Mapping/BoxDecomposition.hpp
        include/tket/Mapping/LexicographicalComparison.hpp
        include/tket/Mapping/LexiLabelling.hpp
        include/tket/Mapping/LexiRoute.hpp
        include/tket/Mapping/LexiRouteRoutingMethod.hpp
        include/tket/Mapping/MappingFrontier.hpp
        include/tket/Mapping/MappingManager.hpp
        include/tket/Mapping/MultiGateReorder.hpp
        include/tket/Mapping/RoutingMethod.hpp
        include/tket/Mapping/RoutingMethodCircuit.hpp
        include/tket/Mapping/RoutingMethodJson.hpp
        include/tket/Mapping/Verification.hpp
        include/tket/MeasurementSetup/MeasurementReduction.hpp
        include/tket/MeasurementSetup/MeasurementSetup.hpp
        include/tket/Ops/BarrierOp.hpp
        include/tket/Ops/ClassicalOps.hpp
        include/tket/Ops/ClExpr.hpp
        include/tket/Ops/FlowOp.hpp
        include/tket/Ops/MetaOp.hpp
        include/tket/Ops/Op.hpp
        include/tket/Ops/OpJsonFactory.hpp
        include/tket/Ops/OpPtr.hpp
        include/tket/OpType/EdgeType.hpp
        include/tket/OpType/OpDesc.hpp
        include/tket/OpType/OpType.hpp
        include/tket/OpType/OpTypeFunctions.hpp
        include/tket/OpType/OpTypeInfo.hpp
        include/tket/PauliGraph/ConjugatePauliFunctions.hpp
        include/tket/PauliGraph/PauliGraph.hpp
        include/tket/Placement/NeighbourPlacements.hpp
        include/tket/Placement/Placement.hpp
        include/tket/Placement/QubitGraph.hpp
        include/tket/Predicates/CompilationUnit.hpp
        include/tket/Predicates/CompilerPass.hpp
        include/tket/Predicates/PassGenerators.hpp
        include/tket/Predicates/PassLibrary.hpp
        include/tket/Predicates/Predicates.hpp
        include/tket/Transformations/BasicOptimisation.hpp
        include/tket/Transformations/CliffordOptimisation.hpp
        include/tket/Transformations/CliffordReductionPass.hpp
        include/tket/Transformations/CliffordResynthesis.hpp
        include/tket/Transformations/Combinator.hpp
        include/tket/Transformations/ContextualReduction.hpp
        include/tket/Transformations/Decomposition.hpp
        include/tket/Transformations/GreedyPauliOptimisation.hpp
        include/tket/Transformations/GreedyPauliOptimisationLookupTables.hpp
        include/tket/Transformations/MeasurePass.hpp
        include/tket/Transformations/OptimisationPass.hpp
        include/tket/Transformations/PauliOptimisation.hpp
        include/tket/Transformations/PhaseOptimisation.hpp
        include/tket/Transformations/PQPSquash.hpp
        include/tket/Transformations/Rebase.hpp
        include/tket/Transformations/Replacement.hpp
        include/tket/Transformations/RzPhasedXSquash.hpp
        include/tket/Transformations/SingleQubitSquash.hpp
        include/tket/Transformations/StandardSquash.hpp
        include/tket/Transformations/ThreeQubitSquash.hpp
        include/tket/Transformations/Transform.hpp
        include/tket/Utils/Constants.hpp
        include/tket/Utils/CosSinDecomposition.hpp
        include/tket/Utils/EigenConfig.hpp
        include/tket/Utils/Expression.hpp
        include/tket/Utils/GraphHeaders.hpp
        include/tket/Utils/HelperFunctions.hpp
        include/tket/Utils/Json.hpp
        include/tket/Utils/MatrixAnalysis.hpp
        include/tket/Utils/PauliTensor.hpp
        include/tket/Utils/SequencedContainers.hpp
        include/tket/Utils/UnitID.hpp
        include/tket/ZX/Flow.hpp
        include/tket/ZX/Rewrite.hpp
        include/tket/ZX/Types.hpp
        include/tket/ZX/ZXDiagram.hpp
        include/tket/ZX/ZXDiagramImpl.hpp
        include/tket/ZX/ZXGenerator.hpp
    )

if (BUILD_TKET_TEST)
    add_subdirectory(test)
endif()
if (BUILD_TKET_PROPTEST)
    add_subdirectory(proptest)
endif()

add_subdirectory(tket_c) # defines the tket_c target

include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/tket)

install(TARGETS tket tket_c
    EXPORT tket-targets
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(MSVC)
    install(TARGETS tket tket_c
        RUNTIME DESTINATION bin
        RUNTIME DESTINATION lib
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
endif()

install(EXPORT tket-targets
    FILE tketTargets.cmake
    NAMESPACE tket::
    DESTINATION ${INSTALL_CONFIGDIR}
)

include(CMakePackageConfigHelpers)

configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/cmake/tketConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/tketConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/tketConfig.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

export(EXPORT tket-targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/tketTargets.cmake
    NAMESPACE tket::)

export(PACKAGE tket)

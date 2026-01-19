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
project(test-tket CXX)

find_package(Boost CONFIG REQUIRED)
find_package(gmp CONFIG)
if (NOT gmp_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(gmp REQUIRED IMPORTED_TARGET gmp)
endif()
find_package(Catch2 CONFIG REQUIRED)
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

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

IF (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /EHsc")
ELSE()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wunreachable-code -Wunused")
ENDIF()

if(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
    # remove -Wno-deprecated-declarations once https://github.com/boostorg/boost/issues/688 is resolved
endif()

add_executable(test-tket
    src/Architecture/test_SubgraphMonomorphisms.cpp
    src/Circuit/test_Boxes.cpp
    src/Circuit/test_Circ.cpp
    src/Circuit/test_CircPool.cpp
    src/Circuit/test_ConjugationBox.cpp
    src/Circuit/test_DiagonalBox.cpp
    src/Circuit/test_DummyBox.cpp
    src/Circuit/test_Multiplexor.cpp
    src/Circuit/test_PauliExpBoxes.cpp
    src/Circuit/test_Slices.cpp
    src/Circuit/test_StatePreparation.cpp
    src/Circuit/test_Symbolic.cpp
    src/Circuit/test_ThreeQubitConversion.cpp
    src/Circuit/test_ToffoliBox.cpp
    src/CircuitsForTesting.cpp
    src/Gate/GatesData.cpp
    src/Gate/test_GateUnitaryMatrix.cpp
    src/Gate/test_Rotation.cpp
    src/Graphs/EdgeSequence.cpp
    src/Graphs/EdgeSequenceColouringParameters.cpp
    src/Graphs/GraphTestingRoutines.cpp
    src/Graphs/RandomGraphGeneration.cpp
    src/Graphs/RandomPlanarGraphs.cpp
    src/Graphs/test_ArticulationPoints.cpp
    src/Graphs/test_DirectedGraph.cpp
    src/Graphs/test_GraphColouring.cpp
    src/Graphs/test_GraphFindComponents.cpp
    src/Graphs/test_GraphFindMaxClique.cpp
    src/Graphs/test_GraphUtils.cpp
    src/Graphs/test_TreeSearch.cpp
    src/Ops/test_ClassicalOps.cpp
    src/Ops/test_Expression.cpp
    src/Ops/test_Ops.cpp
    src/OpType/test_OpTypeFunctions.cpp
    src/Passes/test_CliffordResynthesis.cpp
    src/Passes/test_SynthesiseTK.cpp
    src/Passes/test_SynthesiseTket.cpp
    src/Placement/test_GraphPlacement.cpp
    src/Placement/test_LinePlacement.cpp
    src/Placement/test_NeighbourPlacements.cpp
    src/Placement/test_NoiseAwarePlacement.cpp
    src/Placement/test_Placement.cpp
    src/Simulation/ComparisonFunctions.cpp
    src/Simulation/test_CircuitSimulator.cpp
    src/Simulation/test_PauliExpBoxUnitaryCalculator.cpp
    src/test_AASRoute.cpp
    src/test_ArchitectureAwareSynthesis.cpp
    src/test_Architectures.cpp
    src/test_Assertion.cpp
    src/test_BoxDecompRoutingMethod.cpp
    src/test_ChoiMixTableau.cpp
    src/test_ClExpr.cpp
    src/test_Clifford.cpp
    src/test_Combinators.cpp
    src/test_CompilerPass.cpp
    src/test_Concurrency.cpp
    src/test_ContextOpt.cpp
    src/test_ControlDecomp.cpp
    src/test_DeviceCharacterisation.cpp
    src/test_Diagonalisation.cpp
    src/test_FrameRandomisation.cpp
    src/test_GreedyPauli.cpp
    src/test_json.cpp
    src/test_LexicographicalComparison.cpp
    src/test_LexiRoute.cpp
    src/test_MappingFrontier.cpp
    src/test_MappingManager.cpp
    src/test_MappingVerification.cpp
    src/test_MeasurementReduction.cpp
    src/test_MeasurementSetup.cpp
    src/test_MultiGateReorder.cpp
    src/test_Partition.cpp
    src/test_Path.cpp
    src/test_PauliGraph.cpp
    src/test_PauliTensor.cpp
    src/test_PhaseGadget.cpp
    src/test_PhasePolynomials.cpp
    src/test_Predicates.cpp
    src/test_Rebase.cpp
    src/test_RoundAngles.cpp
    src/test_RoutingMethod.cpp
    src/test_RoutingPasses.cpp
    src/test_SteinerForest.cpp
    src/test_SteinerTree.cpp
    src/test_Synthesis.cpp
    src/test_TwoQubitCanonical.cpp
    src/test_UnitaryTableau.cpp
    src/test_wasm.cpp
    src/tests_main.cpp
    src/testutil.cpp
    src/TokenSwapping/Data/FixedCompleteSolutions.cpp
    src/TokenSwapping/Data/FixedSwapSequences.cpp
    src/TokenSwapping/test_ArchitectureMappingEndToEnd.cpp
    src/TokenSwapping/test_BestTsaFixedSwapSequences.cpp
    src/TokenSwapping/test_DistancesFromArchitecture.cpp
    src/TokenSwapping/test_FullTsa.cpp
    src/TokenSwapping/test_RiverFlowPathFinder.cpp
    src/TokenSwapping/test_SwapsFromQubitMapping.cpp
    src/TokenSwapping/test_VariousPartialTsa.cpp
    src/TokenSwapping/TestUtils/ArchitectureEdgesReimplementation.cpp
    src/TokenSwapping/TestUtils/BestTsaTester.cpp
    src/TokenSwapping/TestUtils/DebugFunctions.cpp
    src/TokenSwapping/TestUtils/DecodedProblemData.cpp
    src/TokenSwapping/TestUtils/FullTsaTesting.cpp
    src/TokenSwapping/TestUtils/GetRandomSet.cpp
    src/TokenSwapping/TestUtils/PartialTsaTesting.cpp
    src/TokenSwapping/TestUtils/ProblemGeneration.cpp
    src/TokenSwapping/TestUtils/TestStatsStructs.cpp
    src/Transformations/test_RedundancyRemoval.cpp
    src/Utils/test_CosSinDecomposition.cpp
    src/Utils/test_HelperFunctions.cpp
    src/Utils/test_MatrixAnalysis.cpp
    src/ZX/test_Flow.cpp
    src/ZX/test_ZXAxioms.cpp
    src/ZX/test_ZXConverters.cpp
    src/ZX/test_ZXDiagram.cpp
    src/ZX/test_ZXExtraction.cpp
    src/ZX/test_ZXRebase.cpp
    src/ZX/test_ZXSimp.cpp
    )

if (NOT TARGET gmp::gmp)
    add_library(gmp::gmp ALIAS PkgConfig::gmp)
endif()
if (NOT TARGET symengine::symengine)
    add_library(symengine::symengine ALIAS symengine)
endif()

target_link_libraries(test-tket PRIVATE Boost::headers)
target_link_libraries(test-tket PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test-tket PRIVATE Eigen3::Eigen)
target_link_libraries(test-tket PRIVATE gmp::gmp)
target_link_libraries(test-tket PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(test-tket PRIVATE symengine::symengine)
target_link_libraries(test-tket PRIVATE tkassert::tkassert)
target_link_libraries(test-tket PRIVATE tket)
target_link_libraries(test-tket PRIVATE tklog::tklog)
target_link_libraries(test-tket PRIVATE tkrng::tkrng)
target_link_libraries(test-tket PRIVATE tktokenswap::tktokenswap)

set(WITH_COVERAGE no CACHE BOOL "Link library with profiling for test coverage")
IF (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(test-tket PUBLIC -pthread)
    IF (WITH_COVERAGE)
        target_link_options(test-tket PUBLIC --coverage)
    ENDIF()
ENDIF()

install(FILES
            src/test_architectures/ibm_montreal.json
            src/test_circuits/bug777_circuit.json
            src/test_circuits/lexiroute_circuit.json
            src/test_circuits/lexiroute_circuit_relabel_to_ancilla.json
        DESTINATION bin)

install(TARGETS test-tket DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )

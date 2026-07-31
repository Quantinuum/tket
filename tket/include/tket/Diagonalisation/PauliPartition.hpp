// Copyright Quantinuum
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <stdexcept>

#include "tket/Utils/GraphHeaders.hpp"
#include "tket/Utils/PauliTensor.hpp"

#include "tket_export.h"

namespace tket {

class UnknownPauliPartitionStrat : public std::logic_error {
 public:
  UnknownPauliPartitionStrat()
      : std::logic_error(
            "Unknown PauliPartitionStrat received when partitioning "
            "Pauli tensors.") {}
};

/**
 * A PauliACGraph is a graph where each vertex is a Pauli tensor, and
 * an edge corresponds to anticommuting tensors
 */

typedef boost::adjacency_list<
    boost::vecS, boost::vecS, boost::undirectedS, SpPauliString>
    PauliACGraph;

typedef boost::graph_traits<PauliACGraph>::vertex_descriptor PauliACVertex;

/**
 * A choice of strategies to partition Pauli tensors into sets
 */
enum class PauliPartitionStrat {
  /**
   * Sets of tensors with no conflicting Paulis; requires no CXs for
   * diagonalisation
   */
  NonConflictingSets,
  /**
   * Sets of mutually commuting tensors; requires O(n^2) CXs for
   * diagonalisation
   */
  CommutingSets
};

/**
 * A choice of methods to perform graph colouring for Pauli partitioning
 */
enum class GraphColourMethod {
  /**
   * Lazy: does not build the graph before performing the colouring;
   * partitions while iterating through the Pauli tensors in the
   * input order.
   */
  Lazy,
  /**
   * Builds the graph and then greedily colours by iterating through
   * the vertices, with the highest degree first.
   */
  LargestFirst,
  /**
   * Builds the graph, then colours it using the minimum possible
   * number of colours. Exponential time in the worst case,
   * but usually returns a result in reasonable time.
   */
  Exhaustive
};

/**
 * A helper class for converting QubitOperator into PauliACGraphs and
 * then colouring the PauliACGraph using some method.
 */
class PauliPartitionerGraph {
 public:
  explicit PauliPartitionerGraph(
      const std::list<SpPauliString>& strings, PauliPartitionStrat strat);

  // KEY: the colour  VALUE: all the Pauli strings assigned that colour.
  std::map<unsigned, std::list<SpPauliString>> partition_paulis(
      GraphColourMethod method) const;

 private:
  PauliACGraph pac_graph;
};

/**
 * Partitions a QubitOperator into lists of mutually commuting gadgets.
 * Assumes that each `SpPauliString` is unique and does not attempt
 * to combine them. If it is given non-unique tensors it will produce
 * inefficient results.
 */
TKET_EXPORT std::list<std::list<SpPauliString>> term_sequence(
    const std::list<SpPauliString>& strings, PauliPartitionStrat strat,
    GraphColourMethod method = GraphColourMethod::Lazy);

}  // namespace tket

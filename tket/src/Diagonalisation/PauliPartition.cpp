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

#include "tket/Diagonalisation/PauliPartition.hpp"

#include <numeric>
#include <tkassert/Assert.hpp>

#include "tket/Graphs/AdjacencyData.hpp"
#include "tket/Graphs/GraphColouring.hpp"

namespace tket {

PauliPartitionerGraph::PauliPartitionerGraph(
    const std::list<SpPauliString>& strings, PauliPartitionStrat strat) {
  pac_graph = {};
  for (const SpPauliString& tensor : strings) {
    PauliACVertex new_vert = boost::add_vertex(tensor, pac_graph);
    BGL_FORALL_VERTICES(v, pac_graph, PauliACGraph) {
      if (v != new_vert) {
        switch (strat) {
          case (PauliPartitionStrat::NonConflictingSets): {
            bool conflict = !tensor.conflicting_qubits(pac_graph[v]).empty();
            if (conflict) boost::add_edge(new_vert, v, pac_graph);
            break;
          }
          case (PauliPartitionStrat::CommutingSets): {
            if (!tensor.commutes_with(pac_graph[v])) {
              boost::add_edge(new_vert, v, pac_graph);
            }
            break;
          }
          default: {
            throw UnknownPauliPartitionStrat();
          }
        }
      }
    }
  }
}

// Consider templatising this and putting it into the graph routines.
// The purpose is to take a boost graph and convert it
// into our format, ready for graph colouring.
// Surely an insignificant overhead unless the graph is tiny.
// MAYBE consider changing our graph colouring routines,
// by templatising/converting to boost-style.
class AbstractGraphData {
 public:
  // All the conversion work is done inside the constructor.
  explicit AbstractGraphData(const PauliACGraph& pac_graph);

  // Return the ID of the string (and also assign a new ID if the string
  // was not seen before); the eventual IDs will form an interval {0,1,2,...,n}.
  std::size_t get_vertex_id(const SpPauliString& pauli_string);

  typedef
      // KEY: the Pauli string present in a vertex
      // VALUE: an integer label for that vertex.
      //      The labels will be a contiguous interval {0,1,2,...,m}.
      std::map<SpPauliString, std::size_t>
          VertexMap;

  const graphs::AdjacencyData& get_adjacency_data() const;

  const VertexMap& get_vertex_map() const;

 private:
  graphs::AdjacencyData m_adjacency_data;
  VertexMap m_vertex_map;
};

std::size_t AbstractGraphData::get_vertex_id(
    const SpPauliString& pauli_string) {
  const auto citer = m_vertex_map.find(pauli_string);
  if (citer != m_vertex_map.cend()) {
    return citer->second;
  }
  // Haven't seen this vertex before!
  const auto new_id = m_vertex_map.size();
  m_vertex_map[pauli_string] = new_id;
  return new_id;
}

const AbstractGraphData::VertexMap& AbstractGraphData::get_vertex_map() const {
  return m_vertex_map;
}

const graphs::AdjacencyData& AbstractGraphData::get_adjacency_data() const {
  return m_adjacency_data;
}

AbstractGraphData::AbstractGraphData(const PauliACGraph& pac_graph)
    : m_adjacency_data(boost::num_vertices(pac_graph)) {
  const auto vertex_iterators = boost::vertices(pac_graph);
  for (auto v_iter = vertex_iterators.first; v_iter != vertex_iterators.second;
       ++v_iter) {
    // NOTE: on Windows, *v_iter appears to be of type uint64,
    // but presumably this is an internal Boost implementation detail
    // and we cannot rely on this.
    // (It might even be that the Boost vertex IDs are contiguous,
    // just like ours, in which case this conversion
    // is unnecessary - but again, we cannot rely on this).
    const auto this_vertex_id = get_vertex_id(pac_graph[*v_iter]);
    const auto neighbour_iterators =
        boost::adjacent_vertices(*v_iter, pac_graph);

    for (auto n_iter = neighbour_iterators.first;
         n_iter != neighbour_iterators.second; ++n_iter) {
      m_adjacency_data.add_edge(
          this_vertex_id, get_vertex_id(pac_graph[*n_iter]));
    }
  }
}

static std::map<unsigned, std::list<SpPauliString>>
get_partitioned_paulis_for_exhaustive_method(const PauliACGraph& pac_graph) {
  const AbstractGraphData data(pac_graph);
  const graphs::GraphColouringResult colouring =
      graphs::GraphColouringRoutines::get_colouring(data.get_adjacency_data());

  TKET_ASSERT(data.get_vertex_map().size() == colouring.colours.size());

  std::map<unsigned, std::list<SpPauliString>> colour_map;

  for (const auto& entry : data.get_vertex_map()) {
    const SpPauliString& vertex = entry.first;
    const std::size_t id = entry.second;
    TKET_ASSERT(id < colouring.colours.size());

    // "id" is the index of this vertex.
    const std::size_t colour = colouring.colours[id];
    TKET_ASSERT(colour < colouring.number_of_colours);

    colour_map[colour].push_back(vertex);
  }
  if (!colour_map.empty()) {
    TKET_ASSERT(colour_map.size() == 1 + colour_map.crbegin()->first);
    TKET_ASSERT(colour_map.cbegin()->first == 0);
  }
  return colour_map;
}

static std::map<unsigned, std::list<SpPauliString>>
get_partitioned_paulis_for_largest_first_method(const PauliACGraph& pac_graph) {
  std::vector<unsigned> order_vec(boost::num_vertices(pac_graph));
  std::iota(order_vec.begin(), order_vec.end(), 0);

  std::sort(order_vec.begin(), order_vec.end(), [&](unsigned i, unsigned j) {
    return (boost::out_degree(i, pac_graph) > boost::out_degree(j, pac_graph));
  });

  /* Some boost machinery */
  std::vector<unsigned> colour_vec(boost::num_vertices(pac_graph));
  boost::iterator_property_map<
      unsigned*,
      boost::property_map<PauliACGraph, boost::vertex_index_t>::const_type>
      colour_prop_map(
          &colour_vec.front(), boost::get(boost::vertex_index, pac_graph));
  boost::sequential_vertex_coloring(
      pac_graph,
      boost::make_iterator_property_map(
          order_vec.begin(), boost::identity_property_map()),
      colour_prop_map);

  std::map<unsigned, std::list<SpPauliString>> colour_map;
  BGL_FORALL_VERTICES(v, pac_graph, PauliACGraph) {
    unsigned v_colour = colour_prop_map[v];
    colour_map[v_colour].push_back(pac_graph[v]);
  }
  return colour_map;
}

std::map<unsigned, std::list<SpPauliString>>
PauliPartitionerGraph::partition_paulis(GraphColourMethod method) const {
  switch (method) {
    case GraphColourMethod::LargestFirst:
      return get_partitioned_paulis_for_largest_first_method(pac_graph);

    case GraphColourMethod::Exhaustive:
      return get_partitioned_paulis_for_exhaustive_method(pac_graph);

    case GraphColourMethod::Lazy:
      throw std::logic_error(
          "Lazy graph colouring should never reach this point");

    default: {
      throw std::logic_error("Unknown graph colouring method");
    }
  }
}

static std::list<std::list<SpPauliString>>
get_term_sequence_for_lazy_colouring_method(
    const std::list<SpPauliString>& strings, PauliPartitionStrat strat) {
  std::list<std::list<SpPauliString>> terms;
  for (const SpPauliString& qpt : strings) {
    if (terms.empty()) {
      terms.push_back({qpt});
      continue;
    }

    bool found_bin = false;
    for (std::list<SpPauliString>& qpt_list : terms) {
      bool viable_bin = true;
      for (const SpPauliString& qpt2 : qpt_list) {
        switch (strat) {
          case (PauliPartitionStrat::NonConflictingSets): {
            bool conflict = !qpt.conflicting_qubits(qpt2).empty();
            if (conflict) viable_bin = false;
            break;
          }
          case (PauliPartitionStrat::CommutingSets): {
            if (!qpt.commutes_with(qpt2)) viable_bin = false;
            break;
          }
          default: {
            throw UnknownPauliPartitionStrat();
          }
        }

        if (viable_bin == false) break;
      }
      if (viable_bin) {
        qpt_list.push_back(qpt);
        found_bin = true;
        break;
      }
    }

    if (found_bin == false) {
      terms.push_back({qpt});
    }
  }
  return terms;
}

static std::list<std::list<SpPauliString>>
get_term_sequence_with_constructed_dependency_graph(
    const std::list<SpPauliString>& strings, PauliPartitionStrat strat,
    GraphColourMethod method) {
  std::list<std::list<SpPauliString>> terms;
  PauliPartitionerGraph pp(strings, strat);
  std::map<unsigned, std::list<SpPauliString>> colour_map =
      pp.partition_paulis(method);

  for (const std::pair<const unsigned, std::list<SpPauliString>>& colour_pair :
       colour_map) {
    terms.push_back(colour_pair.second);
  }
  return terms;
}

std::list<std::list<SpPauliString>> term_sequence(
    const std::list<SpPauliString>& strings, PauliPartitionStrat strat,
    GraphColourMethod method) {
  switch (method) {
    case GraphColourMethod::Lazy:
      return get_term_sequence_for_lazy_colouring_method(strings, strat);

    case GraphColourMethod::LargestFirst:
      // Deliberate fall through
    case GraphColourMethod::Exhaustive:
      return get_term_sequence_with_constructed_dependency_graph(
          strings, strat, method);
    default:
      throw std::logic_error("term_sequence : unknown graph colouring method");
  }
}

}  // namespace tket

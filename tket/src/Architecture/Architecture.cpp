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

#include "tket/Architecture/Architecture.hpp"

#include <boost/graph/biconnected_components.hpp>
#include <tkassert/Assert.hpp>
#include <vector>

#include "tket/Graphs/ArticulationPoints.hpp"
#include "tket/Utils/UnitID.hpp"

namespace tket {

bool Architecture::valid_operation(
    const std::vector<Node>& uids, bool bidirectional) const {
  for (Node n : uids) {
    if (!this->node_exists(Node(n))) return false;
  }
  if (uids.size() == 1) {
    return true;
  } else if (uids.size() == 2) {
    if (bidirectional && this->bidirectional_edge_exists(uids[0], uids[1])) {
      return true;
    } else if (!bidirectional && this->edge_exists(uids[0], uids[1])) {
      return false;
    }
  }
  return false;
}

Architecture Architecture::create_subarch(
    const std::vector<Node>& subarc_nodes) const {
  Architecture subarc(subarc_nodes);
  for (auto [u1, u2] : get_all_edges_vec()) {
    if (subarc.node_exists(u1) && subarc.node_exists(u2)) {
      subarc.add_connection(u1, u2);
    }
  }
  return subarc;
}

// Given a vector of lengths of lines, returns a vector of lines of these sizes
// comprised of architecture nodes
std::vector<node_vector_t> Architecture::get_lines(
    std::vector<unsigned> required_lengths) const {
  // check total length doesn't exceed number of nodes
  bool not_enough_nodes =
      std::accumulate(required_lengths.begin(), required_lengths.end(), 0u) >
      n_nodes();
  TKET_ASSERT(!not_enough_nodes);  // Not enough nodes for required lengths.

  std::sort(
      required_lengths.begin(), required_lengths.end(),
      std::greater<unsigned>());

  UndirectedConnGraph curr_graph(get_undirected_connectivity());
  std::vector<node_vector_t> found_lines;
  for (unsigned length : required_lengths) {
    std::vector<Vertex> longest(
        graphs::longest_simple_path(curr_graph, length));
    if (longest.size() >= length) {
      longest.resize(length);
      // convert Vertex to Node
      node_vector_t to_add;
      for (Vertex v : longest) {
        to_add.push_back(curr_graph[v]);
      }
      found_lines.push_back(to_add);
      for (Vertex v : longest) {
        boost::clear_vertex(v, curr_graph);
      }
    }
  }
  return found_lines;
}

std::set<Node> Architecture::get_articulation_points(
    const Architecture& subarc) const {
  return graphs::get_subgraph_aps<Node>(
      get_undirected_connectivity(), subarc.get_undirected_connectivity());
}

std::set<Node> Architecture::get_articulation_points() const {
  std::set<Vertex> aps;
  UndirectedConnGraph undir_g = get_undirected_connectivity();
  boost::articulation_points(undir_g, std::inserter(aps, aps.begin()));
  std::set<Node> ret;
  for (Vertex ap : aps) {
    ret.insert(undir_g[ap]);
  }
  return ret;
}

static bool lexicographical_comparison(
    const std::vector<std::size_t>& dist1,
    const std::vector<std::size_t>& dist2) {
  return std::lexicographical_compare(
      dist1.begin(), dist1.end(), dist2.begin(), dist2.end());
}

std::optional<Node> Architecture::find_worst_node(
    const Architecture& original_arch) {
  if (n_nodes() == 0) {
    return std::nullopt;
  }
  node_set_t ap = get_articulation_points();
  node_set_t min_nodes = min_degree_nodes();

  std::set<Node> bad_nodes;
  std::set_difference(
      min_nodes.cbegin(), min_nodes.cend(), ap.cbegin(), ap.cend(),
      std::inserter(bad_nodes, bad_nodes.begin()));

  if (bad_nodes.empty()) {
    return std::nullopt;
  }
  auto make_lexicographic = [](std::vector<std::size_t> distances) {
    unsigned max = distances.size();
    std::vector<std::size_t> out(max + 1, std::size_t(0));
    for (const auto& distance : distances) {
      TKET_ASSERT(distance < max);
      out[max - distance]++;
    }
    return out;
  };
  std::vector<std::size_t> worst_distances, temp_distances;
  Node worst_node = *bad_nodes.begin();
  worst_distances = make_lexicographic(get_distances(worst_node));
  for (Node temp_node : bad_nodes) {
    temp_distances = make_lexicographic(get_distances(temp_node));
    int distance_comp =
        tri_lexicographical_comparison(temp_distances, worst_distances);
    if (distance_comp == 1) {
      worst_node = temp_node;
      worst_distances = temp_distances;
    } else if (distance_comp == -1) {
      std::vector<std::size_t> temp_distances_full =
          make_lexicographic(original_arch.get_distances(temp_node));
      std::vector<std::size_t> worst_distances_full =
          make_lexicographic(original_arch.get_distances(worst_node));
      if (lexicographical_comparison(
              temp_distances_full, worst_distances_full)) {
        worst_node = temp_node;
        worst_distances = temp_distances;
      }
    }
  }
  return worst_node;
}

node_set_t Architecture::remove_worst_nodes(unsigned num) {
  node_set_t out;
  Architecture original_arch(*this);
  for (unsigned k = 0; k < num; k++) {
    std::optional<Node> v = find_worst_node(original_arch);
    if (v.has_value()) {
      remove_node(v.value());
      out.insert(v.value());
    }
  }
  return out;
}

int tri_lexicographical_comparison(
    const std::vector<std::size_t>& dist1,
    const std::vector<std::size_t>& dist2) {
  // add assertion that these are the same size distance vectors
  auto start_dist1 = dist1.cbegin();
  auto start_dist2 = dist2.cbegin();

  while (start_dist1 != dist1.cend()) {
    if (start_dist2 == dist2.cend() || *start_dist2 < *start_dist1) {
      return 0;
    } else if (*start_dist1 < *start_dist2) {
      return 1;
    }
    ++start_dist1;
    ++start_dist2;
  }
  return -1;
}

MatrixXb Architecture::get_connectivity() const {
  unsigned n = n_nodes();
  MatrixXb connectivity = MatrixXb(n, n);
  for (unsigned i = 0; i != n; ++i) {
    for (unsigned j = 0; j != n; ++j) {
      connectivity(i, j) =
          edge_exists(Node(i), Node(j)) || edge_exists(Node(j), Node(i));
    }
  }
  return connectivity;
}

void to_json(nlohmann::json& j, const Architecture::Connection& link) {
  j.push_back(link.first);
  j.push_back(link.second);
}

void from_json(const nlohmann::json& j, Architecture::Connection& link) {
  link.first = j.at(0).get<Node>();
  link.second = j.at(1).get<Node>();
}

void to_json(nlohmann::json& j, const Architecture& ar) {
  // Preserve the internal order of ids since Placement depends on this
  Architecture::ConnGraph g = ar.get_directed_connectivity();
  std::vector<Node> nodes;
  for (auto v : boost::make_iterator_range(boost::vertices(g)))
    nodes.push_back(g[v]);
  j["nodes"] = nodes;
  nlohmann::json links;
  for (const Architecture::Connection& con : ar.get_all_edges_vec()) {
    nlohmann::json entry;
    entry["link"] = con;
    entry["weight"] = ar.get_connection_weight(con.first, con.second);
    links.push_back(entry);
  }
  j["links"] = links;
}

void from_json(const nlohmann::json& j, Architecture& ar) {
  for (const Node& n : j.at("nodes").get<node_vector_t>()) {
    ar.add_node(n);
  }
  for (const auto& j_entry : j.at("links")) {
    Architecture::Connection l =
        j_entry.at("link").get<Architecture::Connection>();
    unsigned w = j_entry.at("weight").get<unsigned>();
    ar.add_connection(l.first, l.second, w);
  }
}

void to_json(nlohmann::json& j, const FullyConnected& ar) {
  auto uid_its = ar.nodes();
  std::vector<Node> nodes{uid_its.begin(), uid_its.end()};
  j["nodes"] = nodes;
}

void from_json(const nlohmann::json& j, FullyConnected& ar) {
  for (const Node& n : j.at("nodes").get<node_vector_t>()) {
    ar.add_node(n);
  }
}

//////////////////////////////////////
//      Architecture subclasses     //
//////////////////////////////////////

RingArch::RingArch(unsigned numberOfNodes, const std::string& label)
    : Architecture(get_edges(numberOfNodes, label)) {}

std::vector<Architecture::Connection> RingArch::get_edges(
    unsigned numberOfNodes, const std::string& label) {
  std::vector<Connection> edges;
  for (unsigned i = 0; i < numberOfNodes; i++) {
    Node n1(label, i);
    Node n2(label, (i + 1) % numberOfNodes);
    edges.push_back({n1, n2});
  }
  return edges;
}

SquareGrid::SquareGrid(
    const unsigned dim_r, const unsigned dim_c, const unsigned _layers,
    const std::string& label)
    : Architecture(get_edges(dim_r, dim_c, _layers, label)),
      dimension_r{dim_r},
      dimension_c{dim_c},
      layers{_layers} {}

std::vector<Architecture::Connection> SquareGrid::get_edges(
    const unsigned dim_r, const unsigned dim_c, const unsigned layers,
    const std::string& label) {
  std::vector<Connection> edges;
  for (unsigned l = 0; l < layers; l++) {
    for (unsigned ver = 0; ver < dim_r; ver++) {
      for (unsigned hor = 0; hor < dim_c; hor++) {
        Node n(label, ver, hor, l);
        if (hor != dim_c - 1) {
          Node h_neighbour(label, ver, hor + 1, l);
          edges.push_back({n, h_neighbour});
        }
        if (ver != dim_r - 1) {
          Node v_neighbour(label, ver + 1, hor, l);
          edges.push_back({n, v_neighbour});
        }
        if (l != layers - 1) {
          Node l_neighbour(label, ver, hor, l + 1);
          edges.push_back({n, l_neighbour});
        }
      }
    }
  }
  return edges;
}

}  // namespace tket

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

// This must be the first include since it includes Python.h.
// https://docs.python.org/3/c-api/intro.html#include-files
// clang-format off
#include <nanobind/nanobind.h>
// clang-format on

#include "tket/Placement/Placement.hpp"

#include <memory>

#include "binder_utils.hpp"
#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

void place_with_map(Circuit &circ, std::map<Qubit, Node> &qmap) {
  Architecture arc;
  Placement plobj(arc);
  plobj.place_with_map(circ, qmap);
}

void place_fully_connected(
    Circuit &circ, const FullyConnected &fully_connected) {
  if (circ.n_qubits() > fully_connected.n_nodes()) {
    throw std::logic_error(
        "Circuit has more qubits than the FullyConnected graph has nodes");
  }
  std::map<Qubit, Node> qmap;
  std::vector<Qubit> qubits = circ.all_qubits();
  std::vector<Node> nodes = fully_connected.get_all_nodes_vec();
  TKET_ASSERT(nodes.size() >= qubits.size());
  for (unsigned i = 0; i < qubits.size(); i++) {
    qmap[qubits[i]] = nodes[i];
  }
  place_with_map(circ, qmap);
}

NB_MODULE(placement, m) {
  nb::set_leak_warnings(false);
  nb::class_<Placement>(
            m, "Placement",
            "The base Placement class, contains methods for getting maps "
            "between Circuit Qubits and Architecture Nodes and for relabelling "
            "Circuit Qubits.")

            .def(nb::init<Architecture &>(),
                 "The constructor for a Placement object. The Architecture object "
                 "describes the connectivity between "
                 "qubits.\n\n:param arc: An Architecture object.",
                 nb::arg("arc"))
            .def("__repr__",
                 [](const Placement &) { return "<tket::Placement>"; })
            .def("place",
              [](const Placement &placement, Circuit &circ) {
                return placement.place(circ);
              },
              "Relabels Circuit Qubits to Architecture Nodes and 'unplaced'. For "
              "base Placement, all Qubits and labelled 'unplaced'. "
              "\n\n:param circuit: The Circuit being relabelled.",
              nb::arg("circuit"))
            .def_static(
              "place_with_map",
              [](Circuit &circ, std::map<Qubit, Node>& qmap) {
                return Placement::place_with_map(circ, qmap);
              },
              "Relabels Circuit Qubits to Architecture Nodes using given map. "
              "\n\n:param circuit: The circuit being relabelled\n:param "
              "qmap: The map from logical to physical qubits to apply.",
              nb::arg("circuit"), nb::arg("qmap"))
            .def("get_placement_map", &Placement::get_placement_map,
                 "Returns a map from logical to physical qubits that is Architecture "
                 "appropriate for the given Circuit. "
                 "\n\n:param circuit: The circuit a map is designed for."
                 "\n:return: dictionary mapping " CLSOBJS(~.Qubit) " to "
                 CLSOBJS(~.Node),
                 nb::arg("circuit"))
            .def("get_placement_maps", &Placement::get_all_placement_maps,
                 "Returns a list of maps from logical to physical qubits that "
                 "are Architecture appropriate for the given Circuit. Each map is "
                 "estimated to given a similar SWAP overheard after routing. "
                 "\n\n:param circuit: The circuit the maps are designed for."
                 "\n:param matches: The maximum number of maps returned by the method."
                 "\n:return: list of dictionaries mapping " CLSOBJS(~.Qubit) " "
                 "to " CLSOBJS(~.Node),
                 nb::arg("circuit"), nb::arg("matches")=100)
            .def(
                "to_dict", [](const Placement &placement) {
                    return nb::object(json(std::make_shared<Placement>(placement))); },
                "Return a JSON serializable dict representation of "
                "the Placement."
                "\n\n:return: dict representing the Placement.")
            .def_static(
                "from_dict", [](const nb::dict &dict) {
                    return json(dict).get<Placement::Ptr>(); },
                "Construct Placement instance from JSON serializable "
                "dict representation of the Placement.");

  nb::class_<LinePlacement, Placement>(
      m, "LinePlacement",
      "The LinePlacement class, contains methods for getting maps "
      "between Circuit Qubits and Architecture Nodes and for relabelling "
      "Circuit Qubits.")
      .def(
          nb::init<Architecture &, unsigned, unsigned>(),
          "The constructor for a LinePlacement object. The Architecture "
          "object describes the connectivity "
          "between qubits. In this class, a reduced qubit interaction "
          "subgraph is constructed where each node has maximum outdegree 2 "
          "and does not construct a circle (i.e. lines). "
          "To place the Circuit, a Hamiltonian Path is found in the "
          "Architecture "
          "and this subgraph of lines is assigned along it."
          "\n\n:param arc: An Architecture object."
          "\n:param maximum_line_gates: maximum number of gates in the circuit "
          "considered "
          "when constructing lines for assigning to the graph"
          "\n:param maximum_line_depth: maximum depth of circuit considered "
          "when constructing lines for assigning to the graph",
          nb::arg("arc"), nb::arg("maximum_line_gates") = 100,
          nb::arg("maximum_line_depth") = 100)
      .def(
          "__repr__", [](const Placement &) { return "<tket::LinePlacement>"; })
      .def(
          "to_dict",
          [](const LinePlacement &placement) {
            return nb::object(json(std::make_shared<LinePlacement>(placement)));
          },
          "Return a JSON serializable dict representation of "
          "the LinePlacement."
          "\n\n:return: dict representing the LinePlacement.");

  nb::class_<GraphPlacement, Placement>(
      m, "GraphPlacement",
      "The GraphPlacement class, contains methods for getting maps "
      "between Circuit Qubits and Architecture Nodes and for relabelling "
      "Circuit Qubits.")
      .def(
          nb::init<
              const Architecture &, unsigned, unsigned, unsigned, unsigned>(),
          "The constructor for a GraphPlacement object. The Architecture "
          "object describes the connectivity "
          "between qubits. To find a qubit to node assignment, this method "
          "constructs a pattern graph where vertices are Circuit qubits and "
          "edges mean a pair of qubits have an interaction in the circuit, "
          "and then tries to find a weighted subgraph monomorphsim to the "
          "architecture connectivity, or target, graph. Edges in the pattern "
          "graph are weighted by the circuit depth at which the interaction "
          "between a "
          "pair of qubit occurs. "
          "The number of edges added to the pattern graph is effected by the "
          "maximum_pattern_gates and maximum_pattern_depth arguments. "
          "If no subgraph monomorphism can be found, "
          "lower edge weights are removed from the pattern graph, are more "
          "edges "
          "are added to the target graph. Edges added to the pattern graph are "
          "weighted lower to reflect what the distance between the Nodes they  "
          "are added between was on the original target graph. "
          "\n\n:param arc: An Architecture object.\n"
          ":param maximum_matches: The total number of weighted subgraph "
          "monomorphisms that can be found before matches are "
          "returned.\n"
          ":param timeout: Total time in milliseconds before stopping "
          "search for monomorphisms.\n"
          ":param maximum_pattern_gates: The upper bound on the number of "
          "circuit gates used to construct the pattern graph for finding "
          "subgraph monomorphisms.\n"
          ":param maximum_pattern_depth: The upper bound on the circuit depth "
          "gates "
          "are added to the pattern graph to for finding subgraph "
          "monomorphisms.",
          nb::arg("arc"), nb::arg("maximum_matches") = 1000,
          nb::arg("timeout") = 1000, nb::arg("maximum_pattern_gates") = 100,
          nb::arg("maximum_pattern_depth") = 100)
      .def(
          "__repr__",
          [](const Placement &) { return "<tket::GraphPlacement>"; })
      .def(
          "to_dict",
          [](const GraphPlacement &placement) {
            return nb::object(
                json(std::make_shared<GraphPlacement>(placement)));
          },
          "Return a JSON serializable dict representation of "
          "the GraphPlacement."
          "\n\n:return: dict representing the GraphPlacement.");

  nb::class_<NoiseAwarePlacement, Placement>(
      m, "NoiseAwarePlacement",
      "The NoiseAwarePlacement class, contains methods for getting maps "
      "between Circuit Qubits and Architecture Nodes and for relabelling "
      "Circuit Qubits. It uses gate error rates and readout errors "
      "to find the best placement map.")
      .def(
          nb::init<
              Architecture &, avg_node_errors_t, avg_link_errors_t,
              avg_readout_errors_t, unsigned, unsigned, unsigned, unsigned>(),
          "The constructor for a NoiseAwarePlacement object. The Architecture"
          " object describes the connectivity between qubits. "
          "The dictionaries passed as parameters indicate the average "
          "gate errors for single- and two-qubit gates as well as readout"
          "errors.  If no error is given for a given node or pair of nodes,"
          "the fidelity is assumed to be 1."
          "\n\n:param arc: An Architecture object\n"
          ":param node_errors: a dictionary mapping nodes in the "
          "architecture to average single-qubit gate errors\n"
          ":param link_errors: a dictionary mapping pairs of nodes in the "
          "architecture to average two-qubit gate errors\n"
          ":param readout_errors: a dictionary mapping nodes in the "
          "architecture to average measurement readout errors.\n"
          ":param maximum_matches: The total number of weighted subgraph "
          "monomorphisms that can be found before matches are returned.\n"
          ":param timeout: Total time in milliseconds before stopping search "
          "for monomorphisms.\n"
          ":param maximum_pattern_gates: The upper bound on the number of "
          "circuit gates used to construct the pattern graph for finding "
          "subgraph monomorphisms.\n"
          ":param maximum_pattern_depth: The upper bound on the circuit depth "
          "gates are added to the pattern graph to for finding subgraph "
          "monomorphisms.",
          nb::arg("arc"), nb::arg("node_errors") = nb::dict(),
          nb::arg("link_errors") = nb::dict(),
          nb::arg("readout_errors") = nb::dict(),
          nb::arg("maximum_matches") = 1000, nb::arg("timeout") = 1000,
          nb::arg("maximum_pattern_gates") = 100,
          nb::arg("maximum_pattern_depth") = 100)
      .def(
          "__repr__",
          [](const Placement &) { return "<tket::NoiseAwarePlacement>"; })
      .def(
          "to_dict",
          [](const NoiseAwarePlacement &placement) {
            return nb::object(
                json(std::make_shared<NoiseAwarePlacement>(placement)));
          },
          "Return a JSON serializable dict representation of "
          "the NoiseAwarePlacement."
          "\n\n:return: dict representing the NoiseAwarePlacement.");

  m.def(
      "place_with_map", &place_with_map,
      "Relabels Circuit Qubits according to a map. If provided map "
      "is partial, remaining Circuit Qubits are left 'unplaced'. "
      "\n\n:param circuit: The Circuit being relabelled. \n:param qmap: "
      "The map from logical to physical qubits to apply.",
      nb::arg("circuit"), nb::arg("qmap"));

  m.def(
      "place_fully_connected", &place_fully_connected,
      "Relabels all Circuit Qubits to the Node objects of a FullyConnected "
      "object. "
      "\n\n:param circuit: The Circuit being relabelled. \n:param "
      "fully_connected: "
      "FullyConnected object Qubits being relabelled to match.",
      nb::arg("circuit"), nb::arg("fully_connected"));
}
}  // namespace tket

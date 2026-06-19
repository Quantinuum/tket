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

#include "tket/Architecture/Architecture.hpp"

#include <vector>

#include "deleted_hash.hpp"
#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"
#include "py_operators.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

NB_MODULE(architecture, m) {
  nb::set_leak_warnings(false);
  nb::module_::import_("pytket._tket.unit_id");
  nb::class_<Architecture>(
      m, "Architecture",
      "Class describing the connectivity of qubits on a general device.")
      .def(nb::init<>(), "Produces an empty architecture")
      .def(
          "__init__",
          [](Architecture *p,
             const nb::tket_custom::SequenceVec<std::pair<unsigned, unsigned>>
                 &connections) { new (p) Architecture(connections); },
          "The constructor for an architecture with connectivity "
          "between qubits.\n\n:param connections: A list of pairs "
          "representing qubit indices that can perform two-qubit "
          "operations",
          nb::arg("connections"))
      .def(
          nb::init<
              const nb::tket_custom::SequenceVec<std::pair<Node, Node>> &>(),
          "The constructor for an architecture with connectivity "
          "between qubits.\n\n:param connections: A list of pairs "
          "representing :py:class:`~.Node` s that can perform two-qubit "
          "operations",
          nb::arg("connections"))
      .def(
          "__repr__",
          [](const Architecture &arc) {
            return "<tket::Architecture, nodes=" +
                   std::to_string(arc.n_nodes()) + ">";
          })
      .def(
          "get_distance", &Architecture::get_distance,
          "given two nodes in Architecture, "
          "returns distance between them",
          nb::arg("node_0"), nb::arg("node_1"))
      .def(
          "valid_operation",
          [](const Architecture &arch,
             const nb::tket_custom::SequenceVec<Node> &ids,
             bool bidirectional) {
            return arch.valid_operation(ids, bidirectional);
          },
          "Checks if a given operation on the given nodes can be executed on "
          "the Architecture's connectivity graph.\n"
          "The operation is considered valid if:\n\n"
          "- The operation acts on a single node that belongs to the "
          "Architecture.\n"
          "- The operation acts on two nodes, and either:\n\n"
          "  - `bidirectional` is True and an edge exists between the two "
          "nodes in either direction.\n"
          "  - `bidirectional` is False and an edge exists from uids[0] to "
          "uids[1]."
          "\n\nThe function always returns False if the number of nodes "
          "exceeds 2."
          "\n\n:param uids: list of UnitIDs validity is being checked for."
          "\n:param bidirectional: If True, treats edges in the coupling graph "
          "as bidirectional. Defaults to True.",
          nb::arg("uids"), nb::arg("bidirectional") = true)
      .def(
          "get_adjacent_nodes", &Architecture::get_neighbour_nodes,
          "given a node, returns adjacent nodes in Architecture.",
          nb::arg("node"))
      .def_prop_ro(
          "nodes", &Architecture::get_all_nodes_vec,
          "Returns all nodes of architecture as Node objects.")
      .def_prop_ro(
          "coupling", &Architecture::get_all_edges_vec,
          "Returns the coupling map of the Architecture as "
          "UnitIDs. ")
      .def(
          "to_dict",
          [](const Architecture &arch) {
            return nb::cast<nb::dict>(nb::object(json(arch)));
          },
          "Return a JSON serializable dict representation of "
          "the Architecture."
          "\n\n:return: dict containing nodes and links.")
      .def_static(
          "from_dict",
          [](const nb::dict &architecture_dict) {
            return json(architecture_dict).get<Architecture>();
          },
          "Construct Architecture instance from JSON serializable "
          "dict representation of the Architecture.")
      .def(
          "__getstate__",
          [](const Architecture &circ) {
            return nb::make_tuple(nb::cast<nb::dict>(nb::object(json(circ))));
          })
      .def(
          "__setstate__",
          [](Architecture &arc, const nb::tuple &t) {
            const json j = nb::cast<nb::dict>(t[0]);
            new (&arc) Architecture(j.get<Architecture>());
          })
      // as far as Python is concerned, Architectures are immutable
      .def(
          "__deepcopy__", [](const Architecture &arc,
                             const nb::dict & = nb::dict()) { return arc; })
      .def("__eq__", &py_equals<Architecture>)
      .def("__hash__", &deletedHash<Architecture>, deletedHashDocstring);
  nb::class_<SquareGrid, Architecture>(
      m, "SquareGrid",
      "Inherited Architecture class for qubits arranged in a square lattice of "
      "given number of rows and columns. Qubits are arranged with qubits "
      "values increasing first along rows then along columns i.e. for a "
      "3 x 3 grid:\n\n 0 1 2\n\n 3 4 5\n\n 6 7 8")
      .def(
          "__init__",
          [](SquareGrid *p, unsigned x, unsigned y, const std::string &label) {
            new (p) SquareGrid(x, y, 1, label);
          },
          "The constructor for a Square Grid architecture with some "
          "undirected connectivity between qubits.\n\n:param n_rows: "
          "The number of rows in the grid\n:param n_columns: The number "
          "of columns in the grid\n:param label: Name for Node in SquareGrid "
          "Architecture",
          nb::arg("n_rows"), nb::arg("n_columns"),
          nb::arg("label") = "gridNode")
      .def(
          nb::init<
              const unsigned, const unsigned, const unsigned,
              const std::string>(),
          "The constructor for  a Square Grid architecture with some "
          "undirected connectivity between qubits.\n\n:param n_rows: "
          "The number of rows in the grid\n:param n_columns: The number "
          "of columns in the grid\n:param n_layers: The number of "
          "layers of grids\n:param label: Name for Node in SquareGrid "
          "Architecture",
          nb::arg("n_rows"), nb::arg("n_columns"), nb::arg("n_layers") = 1,
          nb::arg("label") = "gridNode")
      .def(
          "squind_to_qind",
          [](const SquareGrid &self, const unsigned row, const unsigned col) {
            return self.squind_to_qind(row, col);
          },
          "Converts a (row,column) index for a square grid to a "
          "single "
          "qubit index\n\n:param row: The given row index\n:param "
          "column: The given column index\n:return: the "
          "corresponding "
          "global qubit index",
          nb::arg("row"), nb::arg("column"))
      .def(
          "qind_to_squind", &SquareGrid::qind_to_squind,
          "Converts a single qubit index to a (row,column) index for a "
          "square grid.\n\n:param index: The global qubit "
          "index\n:return: the corresponding grid index as a pair "
          "(row,column)",
          nb::arg("index"))
      // as far as Python is concerned, Architectures are immutable
      .def(
          "__deepcopy__",
          [](const SquareGrid &arc, nb::dict = nb::dict()) { return arc; })
      .def("__repr__", [](const SquareGrid &arc) {
        return "<tket::SquareGrid, rows=" + std::to_string(arc.get_rows()) +
               ", columns=" + std::to_string(arc.get_columns()) +
               ", layers=" + std::to_string(arc.get_layers()) + ">";
      });
  nb::class_<RingArch, Architecture>(
      m, "RingArch",
      "Inherited Architecture class for number of qubits arranged in a ring.")
      .def(
          nb::init<const unsigned, const std::string>(),
          "The constructor for a RingArchitecture with some undirected "
          "connectivity between qubits.\n\n:param nodes: number of qubits"
          "\n:param label: Name for Node in RingArch Architecture",
          nb::arg("nodes"), nb::arg("label") = "ringNode")
      .def(
          "__deepcopy__",
          [](const RingArch &arc, nb::dict = nb::dict()) { return arc; })
      .def("__repr__", [](const RingArch &arc) {
        return "<tket::RingArch, nodes=" + std::to_string(arc.n_nodes()) + ">";
      });
  nb::class_<FullyConnected>(
      m, "FullyConnected",
      "A specialised non-Architecture object emulating an architecture with "
      "all qubits connected. "
      "Not compatible with Routing or Placement methods.")
      .def(
          nb::init<unsigned, const std::string>(),
          "Construct a fully-connected architecture."
          "\n\n:param n: number of qubits"
          "\n:param label: Name for Node in "
          "FullyConnected Architecture",
          nb::arg("n"), nb::arg("label") = "fcNode")
      .def(
          "__deepcopy__", [](const FullyConnected &arc,
                             const nb::dict & = nb::dict()) { return arc; })
      .def(
          "__repr__",
          [](const FullyConnected &arc) {
            return "<tket::FullyConnected, nodes=" +
                   std::to_string(arc.n_nodes()) + ">";
          })
      .def("__eq__", &py_equals<FullyConnected>)
      .def("__hash__", &deletedHash<FullyConnected>, deletedHashDocstring)
      .def_prop_ro(
          "nodes", &FullyConnected::get_all_nodes_vec,
          "All nodes of the architecture as :py:class:`~.Node` objects.")
      .def(
          "to_dict",
          [](const FullyConnected &arch) {
            return nb::cast<nb::dict>(nb::object(json(arch)));
          },
          "JSON-serializable dict representation of the architecture."
          "\n\n"
          ":return: dict containing nodes")
      .def_static(
          "from_dict",
          [](const nb::dict &fully_connected_dict) {
            return json(fully_connected_dict).get<FullyConnected>();
          },
          "Construct FullyConnected instance from dict representation.")
      .def(
          "__getstate__",
          [](const FullyConnected &arc) {
            return nb::make_tuple(nb::cast<nb::dict>(nb::object(json(arc))));
          })
      .def("__setstate__", [](FullyConnected &arc, const nb::tuple &t) {
        const json j = nb::cast<nb::dict>(t[0]);
        new (&arc) FullyConnected(j.get<FullyConnected>());
      });
}
}  // namespace tket

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

#include <nanobind/operators.h>

#include "nanobind-stl.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Mapping/AASLabelling.hpp"
#include "tket/Mapping/AASRoute.hpp"
#include "tket/Mapping/BoxDecomposition.hpp"
#include "tket/Mapping/LexiLabelling.hpp"
#include "tket/Mapping/LexiRouteRoutingMethod.hpp"
#include "tket/Mapping/MappingManager.hpp"
#include "tket/Mapping/MultiGateReorder.hpp"
#include "tket/Mapping/RoutingMethodCircuit.hpp"
#include "typecast.hpp"

namespace nb = nanobind;

namespace tket {
NB_MODULE(mapping, m) {
  nb::set_leak_warnings(false);
  nb::module_::import_("pytket._tket.architecture");
  nb::class_<RoutingMethod>(
      m, "RoutingMethod",
      "Parent class for RoutingMethod, for inheritance purposes only, not for "
      "usage.")
      .def(nb::init<>());

  nb::class_<RoutingMethodCircuit, RoutingMethod>(
      m, "RoutingMethodCircuit",
      "The RoutingMethod class captures a method for partially mapping logical "
      "subcircuits to physical operations as permitted by some architecture. "
      "Ranked RoutingMethod objects are used by the MappingManager to route "
      "whole circuits.")
      .def(
          nb::init<
              const std::function<
                  std::tuple<bool, Circuit, unit_map_t, unit_map_t>(
                      const Circuit&, const ArchitecturePtr&)>&,
              unsigned, unsigned>(),
          "Constructor for a routing method defined by partially routing "
          "subcircuits.\n\n:param route_subcircuit: A function declaration "
          "that given a Circuit and Architecture object, returns a tuple "
          "containing a bool informing MappingManager whether to substitute "
          "the returned circuit into the circuit being routed, "
          "a new modified circuit, the initial logical to physical "
          "qubit mapping of the modified circuit and the permutation of "
          "logical to physical qubit mapping given operations in the "
          "modified circuit\n:param max_size: The maximum number of gates "
          "permitted in a subcircuit\n:param max_depth: The maximum permitted "
          "depth of a subcircuit.",
          nb::arg("route_subcircuit"), nb::arg("max_size"),
          nb::arg("max_depth"));

  nb::class_<LexiRouteRoutingMethod, RoutingMethod>(
      m, "LexiRouteRoutingMethod",
      "Defines a RoutingMethod object for mapping circuits that uses the "
      "Lexicographical Comparison approach outlined in arXiv:1902.08091."
      "Only supports 1-qubit, 2-qubit and barrier gates.")
      .def(
          nb::init<unsigned>(),
          "LexiRoute constructor.\n\n:param lookahead: Maximum depth of "
          "lookahead employed when picking SWAP for purpose of logical to "
          "physical mapping.",
          nb::arg("lookahead") = 10);

  nb::class_<AASRouteRoutingMethod, RoutingMethod>(
      m, "AASRouteRoutingMethod",
      "Defines a RoutingMethod object for mapping circuits that uses the "
      "architecture aware synthesis method implemented in tket.")
      .def(
          nb::init<unsigned>(),
          "AASRouteRoutingMethod constructor.\n\n:param aaslookahead: "
          "recursive interaction depth of the architecture aware synthesis."
          "method.",
          nb::arg("aaslookahead"));

  nb::class_<AASLabellingMethod, RoutingMethod>(
      m, "AASLabellingMethod",
      "Defines a Labeling Method for aas for labelling all unplaced qubits in "
      "a circuit")
      .def(nb::init<>(), "AASLabellingMethod constructor.");

  nb::class_<LexiLabellingMethod, RoutingMethod>(
      m, "LexiLabellingMethod",
      "Defines a RoutingMethod for labelling Qubits that uses the "
      "Lexicographical Comparison approach outlined in arXiv:1902.08091.")
      .def(nb::init<>(), "LexiLabellingMethod constructor.");

  nb::class_<MultiGateReorderRoutingMethod, RoutingMethod>(
      m, "MultiGateReorderRoutingMethod",
      "Defines a RoutingMethod object for commuting physically permitted "
      "multi-qubit gates to the front of the subcircuit.")
      .def(
          nb::init<unsigned, unsigned>(),
          "MultiGateReorderRoutingMethod constructor.\n\n:param max_depth: "
          "Maximum number of layers of gates checked for simultaneous "
          "commutation. "
          "\n:param max_size: Maximum number of gates checked for simultaneous "
          "commutation.",
          nb::arg("max_depth") = 10, nb::arg("max_size") = 10);

  nb::class_<BoxDecompositionRoutingMethod, RoutingMethod>(
      m, "BoxDecompositionRoutingMethod",
      "Defines a RoutingMethod object for decomposing boxes.")
      .def(nb::init<>(), "BoxDecompositionRoutingMethod constructor.");

  nb::class_<MappingManager>(
      m, "MappingManager",
      "Defined by a pytket Architecture object, maps Circuit logical qubits "
      "to physically permitted Architecture qubits. Mapping is completed by "
      "sequential routing (full or partial) of subcircuits. A custom method "
      "for routing (full or partial) of subcircuits can be defined in Python.")
      .def(
          nb::init<const ArchitecturePtr&>(),
          "MappingManager constructor.\n\n:param architecture: pytket "
          "Architecture object.",
          nb::arg("architecture"))
      .def(
          "route_circuit",
          [](const MappingManager& self, Circuit& circuit,
             const nb::tket_custom::SequenceVec<RoutingMethodPtr>&
                 routing_methods) {
            return self.route_circuit(circuit, routing_methods);
          },
          "Maps from given logical circuit to physical circuit. Modification "
          "defined by route_subcircuit, but typically this proceeds by "
          "insertion of SWAP gates that permute logical qubits on physical "
          "qubits.\n\n:param circuit: pytket circuit to be mapped"
          "\n:param routing_methods: Ranked methods to use for routing "
          "subcircuits. In given order, each method is sequentially checked "
          "for viability, with the first viable method being used.",
          nb::arg("circuit"), nb::arg("routing_methods"));
}
}  // namespace tket

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

#include "binder_utils.hpp"
#include "nanobind-stl.hpp"
#include "tket/Characterisation/FrameRandomisation.hpp"
#include "tket/Clifford/UnitaryTableau.hpp"
#include "tket/Converters/Converters.hpp"

namespace nb = nanobind;

namespace tket {

SpCxPauliTensor apply_clifford_basis_change_tensor(
    const SpCxPauliTensor &in_pauli, const Circuit &circ) {
  SpPauliStabiliser in_string(in_pauli.string);
  UnitaryRevTableau tab = circuit_to_unitary_rev_tableau(circ);
  SpCxPauliTensor new_operator = tab.get_row_product(in_string);
  new_operator.coeff *= in_pauli.coeff;
  return new_operator;
}

SpPauliString apply_clifford_basis_change_string(
    const SpPauliString &in_pauli, const Circuit &circ) {
  UnitaryRevTableau tab = circuit_to_unitary_rev_tableau(circ);
  SpPauliStabiliser new_operator =
      tab.get_row_product(SpPauliStabiliser(in_pauli));
  return SpPauliString(new_operator.string);
}

NB_MODULE(tailoring, m) {
  nb::set_leak_warnings(false);
  m.doc() = "The tailoring module provides access to noise tailoring tools.";

  nb::class_<FrameRandomisation>(
      m, "FrameRandomisation",
      "The base FrameRandomisation class. FrameRandomisation finds "
      "subcircuits (cycles) of a given circuit comprised of gates with "
      "OpType only from a specified set of OpType, and wires gates into "
      "the boundary (frame) of these cycles. Input frame gates are "
      "sampled from another set of OpType, and output frame gates "
      "deduced such that the circuit unitary doesn't change, achieved by "
      "computing the action of cycle gates on frame gates.")
      .def(
          "__init__",
          [](FrameRandomisation *p, const OpTypeSet &_cycle_types,
             const OpTypeSet &_frame_types,
             const std::map<OpType, std::map<nb::tuple, nb::tuple>>
                 &cycle_frame_actions) {
            std::map<OpType, std::map<OpTypeVector, OpTypeVector>>
                real_cycle_frame_actions;
            for (const auto &actions : cycle_frame_actions) {
              std::map<OpTypeVector, OpTypeVector> otv_map;
              for (const auto &tups : actions.second) {
                otv_map[nb::cast<OpTypeVector>(tups.first)] =
                    nb::cast<OpTypeVector>(tups.second);
              }
              real_cycle_frame_actions[actions.first] = otv_map;
            }
            new (p) FrameRandomisation(
                _cycle_types, _frame_types, real_cycle_frame_actions);
          },
          "Constructor for FrameRandomisation."
          "\n\n:param cycletypes: "
          "A set of OpType corresponding to the gates cycles found are "
          "comprised of"
          "\n:param frametypes: A set of OpType "
          "corresponding to the gates Frames are sampled from"
          "\n:param conjugates: A map from cycle OpType, to a map "
          "between Frame OptypeVector giving the required change to "
          "output frame OpType to preserve Unitary from given input "
          "frame OpType.")
      .def(
          "get_all_circuits", &FrameRandomisation::get_all_circuits,
          "For given circuit, finds all Cycles, finds all frames for "
          "each Cycle, and returns every combination of frame and cycle "
          "in a vector of Circuit.\n\n:param circuit: The circuit to "
          "find frames for.\n"
          ":return: list of " CLSOBJS(~.Circuit),
          nb::arg("circuit"))
      .def(
          "sample_circuits", &FrameRandomisation::sample_randomisation_circuits,
          "Returns a number of instances equal to sample of frame "
          "randomisation for the given circuit. Samples individual "
          "frame gates uniformly.\n\n:param "
          "circuit: The circuit to perform frame randomisation with "
          "Pauli gates on\n:param samples: the number of frame "
          "randomised circuits to return.\n"
          ":return: list of " CLSOBJS(~.Circuit),
          nb::arg("circuit"), nb::arg("samples"))
      .def("__repr__", &FrameRandomisation::to_string);

  nb::class_<PauliFrameRandomisation>(
      m, "PauliFrameRandomisation",
      "The PauliFrameRandomisation class. PauliFrameRandomisation finds "
      "subcircuits (cycles) of a given circuit comprised of gates with "
      "OpType::H, OpType::CX and OpType::S, and wires gates "
      "into "
      "the boundary (frame) of these cycles. Input frame gates are "
      "sampled from another set of OpType comprised of the Pauli gates, "
      "and output frame gates "
      "deduced such that the circuit unitary doesn't change, achieved by "
      "computing the action of cycle gates on frame gates.")
      .def(nb::init<>(), "Constructor for PauliFrameRandomisation.")
      .def(
          "get_all_circuits", &PauliFrameRandomisation::get_all_circuits,
          "For given circuit, finds all Cycles, finds all frames for "
          "each Cycle, and returns every combination of frame and cycle "
          "in a vector of Circuit.\n\n:param circuit: The circuit to "
          "find frames for.\n"
          ":return: list of " CLSOBJS(~.Circuit),
          nb::arg("circuit"))
      .def(
          "sample_circuits",
          &PauliFrameRandomisation::sample_randomisation_circuits,
          "Returns a number of instances equal to sample of frame "
          "randomisation for the given circuit. Samples individual "
          "frame gates uniformly from the Pauli gates.\n\n:param "
          "circuit: The circuit to perform frame randomisation with "
          "Pauli gates on\n:param samples: the number of frame "
          "randomised circuits to return.\n"
          ":return: list of " CLSOBJS(~.Circuit),
          nb::arg("circuit"), nb::arg("samples"))
      .def("__repr__", &PauliFrameRandomisation::to_string);

  nb::class_<UniversalFrameRandomisation>(
      m, "UniversalFrameRandomisation",
      "The UniversalFrameRandomisation class. "
      "UniversalFrameRandomisation finds "
      "subcircuits (cycles) of a given circuit comprised of gates with "
      "OpType::H, OpType::CX, and OpType::Rz, and wires gates "
      "into "
      "the boundary (frame) of these cycles. Input frame gates are "
      "sampled from another set of OpType comprised of the Pauli gates, "
      "and output frame gates "
      "deduced such that the circuit unitary doesn't change, achieved by "
      "computing the action of cycle gates on frame gates. Some gates "
      "with OpType::Rz may be substituted for their dagger to achieve "
      "this.")
      .def(nb::init<>(), "Constructor for UniversalFrameRandomisation.")
      .def(
          "get_all_circuits", &UniversalFrameRandomisation::get_all_circuits,
          "For given circuit, finds all Cycles, finds all frames for "
          "each Cycle, and returns every combination of frame and cycle "
          "in a vector of Circuit.\n\n:param circuit: The circuit to "
          "find frames for.\n"
          ":return: list of " CLSOBJS(~.Circuit),
          nb::arg("circuit"))
      .def(
          "sample_circuits",
          &UniversalFrameRandomisation::sample_randomisation_circuits,
          "Returns a number of instances equal to sample of frame "
          "randomisation for the given circuit. Samples individual "
          "frame gates uniformly from the Pauli gates.\n\n:param "
          "circuit: The circuit to perform frame randomisation with "
          "Pauli gates on\n:param samples: the number of frame "
          "randomised circuits to return.\n"
          ":return: list of " CLSOBJS(~.Circuit),
          nb::arg("circuit"), nb::arg("samples"))
      .def("__repr__", &UniversalFrameRandomisation::to_string);
  m.def(
      "apply_clifford_basis_change", &apply_clifford_basis_change_string,
      "Given Pauli operator P and Clifford circuit C, "
      "returns C_dagger.P.C in multiplication order. This ignores any -1 "
      "phase that could be introduced. "
      "\n\n:param pauli: Pauli operator being transformed. "
      "\n:param circuit: Clifford circuit acting on Pauli operator. "
      "\n:return: :py:class:`~.QubitPauliString` for new operator",
      nb::arg("pauli"), nb::arg("circuit"));
  m.def(
      "apply_clifford_basis_change_tensor", &apply_clifford_basis_change_tensor,
      "Given Pauli operator P and Clifford circuit C, "
      "returns C_dagger.P.C in multiplication order"
      "\n\n:param pauli: Pauli operator being transformed."
      "\n:param circuit: Clifford circuit acting on Pauli operator. "
      "\n:return: :py:class:`~.QubitPauliTensor` for new operator",
      nb::arg("pauli"), nb::arg("circuit"));
}
}  // namespace tket

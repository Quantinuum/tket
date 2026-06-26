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

#include <nanobind/eigen/dense.h>

#include <sstream>

#include "nanobind-stl.hpp"
#include "tket/Clifford/UnitaryTableau.hpp"
#include "tket/Converters/Converters.hpp"
#include "tket/Converters/UnitaryTableauBox.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
namespace tket {

typedef nb::tket_custom::SequenceVec<Qubit> py_qubit_vector_t;
NB_MODULE(tableau, m) {
  nb::set_leak_warnings(false);
  nb::class_<UnitaryTableau>(
      m, "UnitaryTableau",
      "Stabilizer tableau for a unitary in the style of Aaronson&Gottesman "
      "\"Improved Simulation of Stabilizer Circuits\": rows indicate the "
      "action at the output corresponding to either an X or a Z on a single "
      "input.")
      .def(
          nb::init<unsigned>(),
          "Constructs a :py:class:`~.UnitaryTableau` representing the identity "
          "operation over some number of qubits. Qubits will be indexed "
          "sequentially in the default register."
          "\n\n:param nqb: The number of qubits in the unitary.",
          nb::arg("nqb"))
      .def(
          nb::init<
              const MatrixXb&, const MatrixXb&, const VectorXb&,
              const MatrixXb&, const MatrixXb&, const VectorXb&>(),
          "Constructs a :py:class:`~.UnitaryTableau` from the binary tables of "
          "its components."
          "\n\n:param xx: The X component of the X rows."
          "\n:param xz: The Z component of the X rows."
          "\n:param xph: The phases of the X rows."
          "\n:param zx: The X component of the Z rows."
          "\n:param zz: The Z component of the Z rows."
          "\n:param zph: The phases of the Z rows.",
          nb::arg("xx"), nb::arg("xz"), nb::arg("xph"), nb::arg("zx"),
          nb::arg("zz"), nb::arg("zph"))
      .def(
          "__init__",
          [](UnitaryTableau* p, const Circuit& circ) {
            new (p) UnitaryTableau(circuit_to_unitary_tableau(circ));
          },
          "Constructs a :py:class:`~.UnitaryTableau` from a unitary "
          ":py:class:`~.Circuit`. Throws an exception if the input contains "
          "non-unitary operations."
          "\n\n:param circ: The unitary circuit to convert to a tableau.")
      .def(
          "__repr__",
          [](const UnitaryTableau& tab) {
            std::stringstream str;
            str << tab;
            return str.str();
          })
      .def(
          "get_xrow",
          [](const UnitaryTableau& tab, const Qubit& qb) {
            return SpCxPauliTensor(tab.get_xrow(qb));
          },
          "Read off an X row as a Pauli string."
          "\n\n:param qb: The qubits whose X row to read off."
          "\n:return: The Pauli string :math:`P` such that :math:`PU=UX_{qb}`.",
          nb::arg("qb"))
      .def(
          "get_zrow",
          [](const UnitaryTableau& tab, const Qubit& qb) {
            return SpCxPauliTensor(tab.get_zrow(qb));
          },
          "Read off an Z row as a Pauli string."
          "\n\n:param qb: The qubits whose Z row to read off."
          "\n:return: The Pauli string :math:`P` such that :math:`PU=UZ_{qb}`.",
          nb::arg("qb"))
      .def(
          "get_row_product",
          [](const UnitaryTableau& tab, const SpCxPauliTensor& paulis) {
            SpCxPauliTensor res =
                tab.get_row_product(SpPauliStabiliser(paulis.string));
            res.coeff *= paulis.coeff;
            return res;
          },
          "Combine rows to yield the effect of a given Pauli string."
          "\n\n:param paulis: The Pauli string :math:`P` to consider at the "
          "input."
          "\n:return: The Pauli string :math:`Q` such that :math:`QU=UP`.",
          nb::arg("paulis"))
      .def(
          "apply_gate_at_front",
          [](UnitaryTableau& self, const OpType& type,
             const py_qubit_vector_t& qbs) {
            return self.apply_gate_at_front(type, qbs);
          },
          "Update the tableau according to adding a Clifford gate before the "
          "current unitary, i.e. updates :math:`U` to :math:`UG` for a gate "
          ":math:`G`."
          "\n\n:param type: The :py:class:`~.OpType` of the gate to add. Must "
          "be "
          "an unparameterised Clifford gate type."
          "\n:param qbs: The qubits to apply the gate to. Length must match "
          "the arity of the given gate type.",
          nb::arg("type"), nb::arg("qbs"))
      .def(
          "apply_gate_at_end",
          [](UnitaryTableau& self, const OpType& type,
             const py_qubit_vector_t& qbs) {
            return self.apply_gate_at_end(type, qbs);
          },
          "Update the tableau according to adding a Clifford gate after the "
          "current unitary, i.e. updates :math:`U` to :math:`GU` for a gate "
          ":math:`G`."
          "\n\n:param type: The :py:class:`~.OpType` of the gate to add. Must "
          "be "
          "an unparameterised Clifford gate type."
          "\n:param qbs: The qubits to apply the gate to. Length must match "
          "the arity of the given gate type.",
          nb::arg("type"), nb::arg("qbs"))
      .def(
          "to_circuit", &unitary_tableau_to_circuit,
          "Synthesises a unitary :py:class:`~.Circuit` realising the same "
          "unitary as the tableau. Uses the method from Aaronson & Gottesman: "
          "\"Improved Simulation of Stabilizer Circuits\", Theorem 8. This is "
          "not optimised for gate count, so is not recommended for "
          "performance-sensitive usage.");
  nb::class_<UnitaryRevTableau>(
      m, "UnitaryRevTableau",
      "Equivalent to the UnitaryTableau, except that the rows indicate the "
      "action at the input corresponding to either an X or a Z on a single "
      "output.")
      .def(
          nb::init<unsigned>(),
          "Constructs a :py:class:`~.UnitaryRevTableau` representing the "
          "identity "
          "operation over some number of qubits. Qubits will be indexed "
          "sequentially in the default register."
          "\n\n:param nqb: The number of qubits in the unitary.",
          nb::arg("nqb"))
      .def(
          nb::init<
              const MatrixXb&, const MatrixXb&, const VectorXb&,
              const MatrixXb&, const MatrixXb&, const VectorXb&>(),
          "Constructs a :py:class:`~.UnitaryRevTableau` from the binary tables "
          "of "
          "its components."
          "\n\n:param xx: The X component of the X rows."
          "\n:param xz: The Z component of the X rows."
          "\n:param xph: The phases of the X rows."
          "\n:param zx: The X component of the Z rows."
          "\n:param zz: The Z component of the Z rows."
          "\n:param zph: The phases of the Z rows.",
          nb::arg("xx"), nb::arg("xz"), nb::arg("xph"), nb::arg("zx"),
          nb::arg("zz"), nb::arg("zph"))
      .def(
          "__init__",
          [](UnitaryRevTableau* p, const Circuit& circ) {
            new (p) UnitaryRevTableau(circuit_to_unitary_rev_tableau(circ));
          },
          "Constructs a :py:class:`~.UnitaryRevTableau` from a unitary "
          ":py:class:`~.Circuit`. Throws an exception if the input contains "
          "non-unitary operations."
          "\n\n:param circ: The unitary circuit to convert to a tableau.")
      .def(
          "__repr__",
          [](const UnitaryRevTableau& tab) {
            std::stringstream str;
            str << tab;
            return str.str();
          })
      .def(
          "get_xrow",
          [](const UnitaryRevTableau& tab, const Qubit& qb) {
            return SpCxPauliTensor(tab.get_xrow(qb));
          },
          "Read off an X row as a Pauli string."
          "\n\n:param qb: The qubits whose X row to read off."
          "\n:return: The Pauli string :math:`P` such that :math:`UP=X_{qb}U`.",
          nb::arg("qb"))
      .def(
          "get_zrow",
          [](const UnitaryRevTableau& tab, const Qubit& qb) {
            return SpCxPauliTensor(tab.get_zrow(qb));
          },
          "Read off an Z row as a Pauli string."
          "\n\n:param qb: The qubits whose Z row to read off."
          "\n:return: The Pauli string :math:`P` such that :math:`UP=Z_{qb}U`.",
          nb::arg("qb"))
      .def(
          "get_row_product",
          [](const UnitaryRevTableau& tab, const SpCxPauliTensor& paulis) {
            SpCxPauliTensor res =
                tab.get_row_product(SpPauliStabiliser(paulis.string));
            res.coeff *= paulis.coeff;
            return res;
          },
          "Combine rows to yield the effect of a given Pauli string."
          "\n\n:param paulis: The Pauli string :math:`P` to consider at the "
          "output."
          "\n:return: The Pauli string :math:`Q` such that :math:`UQ=PU`.",
          nb::arg("paulis"))
      .def(
          "apply_gate_at_front",
          [](UnitaryRevTableau& self, const OpType& type,
             const py_qubit_vector_t& qbs) {
            return self.apply_gate_at_front(type, qbs);
          },
          "Update the tableau according to adding a Clifford gate before the "
          "current unitary, i.e. updates :math:`U` to :math:`UG` for a gate "
          ":math:`G`."
          "\n\n:param type: The :py:class:`~.OpType` of the gate to add. Must "
          "be "
          "an unparameterised Clifford gate type."
          "\n:param qbs: The qubits to apply the gate to. Length must match "
          "the arity of the given gate type.",
          nb::arg("type"), nb::arg("qbs"))
      .def(
          "apply_gate_at_end",
          [](UnitaryRevTableau& self, const OpType& type,
             const py_qubit_vector_t& qbs) {
            return self.apply_gate_at_end(type, qbs);
          },
          "Update the tableau according to adding a Clifford gate after the "
          "current unitary, i.e. updates :math:`U` to :math:`GU` for a gate "
          ":math:`G`."
          "\n\n:param type: The :py:class:`~.OpType` of the gate to add. Must "
          "be "
          "an unparameterised Clifford gate type."
          "\n:param qbs: The qubits to apply the gate to. Length must match "
          "the arity of the given gate type.",
          nb::arg("type"), nb::arg("qbs"))
      .def(
          "to_circuit", &unitary_rev_tableau_to_circuit,
          "Synthesises a unitary :py:class:`~.Circuit` realising the same "
          "unitary as the tableau. Uses the method from Aaronson & Gottesman: "
          "\"Improved Simulation of Stabilizer Circuits\", Theorem 8. This is "
          "not optimised for gate count, so is not recommended for "
          "performance-sensitive usage.");
  nb::class_<UnitaryTableauBox, Op>(
      m, "UnitaryTableauBox",
      "A Clifford unitary specified by its actions on Paulis.")
      .def(
          nb::init<const UnitaryTableau&>(),
          "Construct from a given tableau.\n\n"
          ":param tab: The :py:class:`~.UnitaryTableau` representing the "
          "desired "
          "unitary.",
          nb::arg("tab"))
      .def(
          nb::init<
              const MatrixXb&, const MatrixXb&, const VectorXb&,
              const MatrixXb&, const MatrixXb&, const VectorXb&>(),
          "Construct the tableau from the binary tables of its components."
          "\n\n:param xx: The X component of the X rows."
          "\n:param xz: The Z component of the X rows."
          "\n:param xph: The phases of the X rows."
          "\n:param zx: The X component of the Z rows."
          "\n:param zz: The Z component of the Z rows."
          "\n:param zph: The phases of the Z rows.",
          nb::arg("xx"), nb::arg("xz"), nb::arg("xph"), nb::arg("zx"),
          nb::arg("zz"), nb::arg("zph"))
      .def(
          "get_circuit",
          [](UnitaryTableauBox& ubox) { return *ubox.to_circuit(); },
          ":return: The :py:class:`~.Circuit` described by the box.")
      .def(
          "get_tableau", &UnitaryTableauBox::get_tableau,
          ":return: The tableau representing the unitary operation.");
}

}  // namespace tket

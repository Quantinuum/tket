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

#include <nanobind/eigen/dense.h>
#include <nanobind/make_iterator.h>
#include <nanobind/nanobind.h>
#include <nanobind/operators.h>

#include <optional>
#include <sstream>
#include <utility>

#include "UnitRegister.hpp"
#include "boost/graph/iteration_macros.hpp"
#include "circuit_registers.hpp"
#include "deleted_hash.hpp"
#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"
#include "py_operators.hpp"
#include "tket/Circuit/Boxes.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/Command.hpp"
#include "tket/Circuit/DummyBox.hpp"
#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Circuit/ToffoliBox.hpp"
#include "tket/Mapping/Verification.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

const bit_vector_t no_bits;

typedef std::variant<UnitID, Qubit, Bit> PyUnitID;
UnitID to_cpp_unitid(const PyUnitID &py_unitid) {
  if (holds_alternative<UnitID>(py_unitid)) {
    return get<UnitID>(py_unitid);
  }
  if (holds_alternative<Qubit>(py_unitid)) {
    return get<Qubit>(py_unitid);
  }
  return get<Bit>(py_unitid);
}

void init_circuit_add_op(nb::class_<Circuit> &c);
void init_circuit_add_classical_op(nb::class_<Circuit> &c);

void def_circuit(nb::class_<Circuit> &pyCircuit) {
  init_circuit_add_op(pyCircuit);
  init_circuit_add_classical_op(pyCircuit);
  pyCircuit
      .def(nb::init<>(), "Constructs a circuit with a completely empty DAG.")
      .def(
          nb::init<const std::string &>(),
          "Constructs a named circuit with a completely empty DAG."
          "\n\n:param name: name for the circuit",
          nb::arg("name"))
      .def(
          nb::init<unsigned, std::optional<std::string>>(),
          "Constructs a circuit with a given number of qubits/blank "
          "wires.\n\n>>> c = Circuit()\n>>> c.add_blank_wires(3)\n\nis "
          "equivalent to\n\n>>> c = Circuit(3)\n\n:param n_qubits: The "
          "number of qubits in the circuit\n:param name: Optional name "
          "for the circuit.",
          nb::arg("n_qubits"), nb::arg("name") = std::nullopt)
      .def(
          nb::init<unsigned, unsigned, std::optional<std::string>>(),
          "Constructs a circuit with a given number of quantum and "
          "classical bits\n\n:param n_qubits: The number of qubits in "
          "the circuit\n:param n_bits: The number of classical bits in "
          "the circuit\n:param name: Optional name for the circuit.",
          nb::arg("n_qubits"), nb::arg("n_bits"),
          nb::arg("name") = std::nullopt)
      .def("__eq__", &py_equals<Circuit>)
      .def("__hash__", &deletedHash<Circuit>, deletedHashDocstring)
      .def(
          "__str__",
          [](const Circuit &circ) {
            return "<tket::Circuit, qubits=" + std::to_string(circ.n_qubits()) +
                   ", gates=" + std::to_string(circ.n_gates()) + ">";
          })
      .def(
          "__repr__",
          [](const Circuit &circ) {
            std::stringstream ss;
            ss << "[";
            for (const auto &q : circ.created_qubits()) {
              ss << "Create " << q.repr() << "; ";
            }
            for (const auto &com : circ.get_commands()) {
              ss << com.to_str() << " ";
            }
            for (const auto &q : circ.discarded_qubits()) {
              ss << "Discard " << q.repr() << "; ";
            }
            ss << "]";
            return ss.str();
          })
      .def(
          "__iter__",
          [](const Circuit &circ) {
            return nb::make_iterator(
                nb::type<Circuit>(), "iterator", circ.begin(), circ.end());
          },
          "Iterate through the circuit, a Command at a time.",
          nb::keep_alive<
              0, 1>() /* Essential: keep object alive while iterator exists */)
      .def(
          "get_commands",
          [](const Circuit &circ) {
            std::vector<Command> out;
            for (Command c : circ) out.push_back(c);
            return out;
          },
          ":return: a list of all the Commands in the circuit")
      .def(
          "get_unitary",
          [](const Circuit &circ) { return tket_sim::get_unitary(circ); },
          ":return: The numerical unitary matrix of the circuit, using ILO-BE "
          "convention.")
      .def(
          "get_unitary_times_other",
          [](const Circuit &circ, Eigen::MatrixXcd matr) {
            tket_sim::apply_unitary(circ, matr);
            return matr;
          },
          "Calculate UM, where U is the numerical unitary matrix of "
          "the circuit, with ILO-BE convention, and M is another matrix. "
          "This is more efficient than calculating U separately, if M has "
          "fewer columns than U."
          "\n\n:param matr: The matrix to be multiplied."
          "\n:return: The product of the circuit unitary and the given matrix.",
          nb::arg("matr"))
      .def(
          "get_statevector",
          [](const Circuit &circ) { return tket_sim::get_statevector(circ); },
          "Calculate the statevector resulting from applying the circuit to "
          "the all-zero state, using ILO-BE convention. The result is a one-"
          "dimensional array."
          "\n\n:return: The calculated vector.")
      .def(
          "add_q_register",
          [](Circuit &circ, const std::string &name, const std::size_t &size) {
            circ.add_q_register(name, size);
            return QubitRegister(name, size);
          },
          "Constructs a new quantum register with a given name and "
          "number of qubits.\n\n:param name: Unique readable name for "
          "the register\n:param size: Number of qubits "
          "required\n:return: a map from index to the corresponding "
          "UnitIDs",
          nb::arg("name"), nb::arg("size"))
      .def(
          "add_q_register",
          [](Circuit &circ, const QubitRegister &reg) {
            const std::string &name = reg.name();
            const std::size_t &size = reg.size();
            register_t existing = circ.get_reg(name);

            if (!existing.empty()) {
              if (existing.size() != size) {
                throw CircuitInvalidity(
                    "Existing register with name \"" + name +
                    "\" already exists, and does not match "
                    "requested size.");
              }
              return reg;
            }
            circ.add_q_register(name, size);
            return reg;
          },
          "Adds QubitRegister to Circuit"
          "\n\n:param register: QubitRegister ",
          nb::arg("register"))
      .def(
          "_add_w_register",
          [](Circuit &circ, const std::size_t &size) {
            return circ.add_wasm_register(size);
          },
          "Creates given number of wasm bits in the circuit. If "
          "there are already wasm bits in circuit only the "
          "additional wasm bits will be added. "
          "\n\n:param size: Number of wasm bits that "
          "should be added to the circuit",
          nb::arg("size"))
      .def(
          "_add_r_register",
          [](Circuit &circ, const std::size_t &size) {
            return circ.add_rng_register(size);
          },
          "Creates given number of RNG wires in the circuit. If "
          "there are already RNG wires in circuit, only the "
          "additional RNG wires will be added."
          "\n\n:param size: Number of RNG wires that "
          "should be added to the circuit",
          nb::arg("size"))
      .def(
          "add_c_register",
          [](Circuit &circ, const std::string &name, const std::size_t &size) {
            circ.add_c_register(name, size);
            return BitRegister(name, size);
          },
          "Constructs a new classical register with a given name and "
          "number of bits.\n\n:param name: Unique readable name for the "
          "register\n:param size: Number of bits required\n:return: a "
          "map from index to the corresponding UnitIDs",
          nb::arg("name"), nb::arg("size"))
      .def(
          "add_c_register",
          [](Circuit &circ, const BitRegister &reg) {
            const std::string &name = reg.name();
            const std::size_t &size = reg.size();
            register_t existing = circ.get_reg(name);

            if (!existing.empty()) {
              if (existing.size() != size) {
                throw CircuitInvalidity(
                    "Existing register with name \"" + name +
                    "\" already exists, and does not match "
                    "requested size.");
              }
              return reg;
            }
            circ.add_c_register(name, size);
            return reg;
          },
          "Adds BitRegister to Circuit"
          "\n\n:param register: BitRegister ",
          nb::arg("register"))
      .def(
          "get_c_register",
          [](Circuit &circ, const std::string &name) {
            register_t reg = circ.get_reg(name);
            if (reg.empty() || reg.begin()->second.type() != UnitType::Bit) {
              throw CircuitInvalidity(
                  "Cannot find classical register with name \"" + name + "\".");
            }
            return BitRegister(name, reg.size());
          },
          "Get the classical register with the given name.\n\n:param name: "
          "name for the register\n:return: the retrieved "
          ":py:class:`~.BitRegister`",
          nb::arg("name"))
      .def_prop_ro(
          "c_registers", &get_unit_registers<BitRegister>,
          "Get all classical registers.\n\n"
          "The list only includes registers that are singly-indexed "
          "contiguously from zero.\n\n"
          ":return: List of :py:class:`~.BitRegister`")
      .def(
          "get_q_register",
          [](Circuit &circ, const std::string &name) {
            register_t reg = circ.get_reg(name);
            if (reg.empty() || reg.begin()->second.type() != UnitType::Qubit) {
              throw CircuitInvalidity(
                  "Cannot find quantum register with name \"" + name + "\".");
            }
            return QubitRegister(name, reg.size());
          },
          "Get the quantum register with the given name.\n\n:param name: "
          "name for the register\n:return: the retrieved "
          ":py:class:`~.QubitRegister`",
          nb::arg("name"))
      .def_prop_ro(
          "q_registers", &get_unit_registers<QubitRegister>,
          "Get all quantum registers.\n\n"
          "The list only includes registers that are singly-indexed "
          "contiguously from zero.\n\n"
          ":return: List of :py:class:`~.QubitRegister`")
      .def(
          "add_qubit", &Circuit::add_qubit,
          "Constructs a single qubit with the given id.\n\n:param id: "
          "Unique id for the qubit\n:param reject_dups: Fail if there "
          "is already a qubit in this circuit with the id. Default to "
          "True",
          nb::arg("id"), nb::arg("reject_dups") = true)
      .def(
          "add_bit", &Circuit::add_bit,
          "Constructs a single bit with the given id.\n\n:param id: "
          "Unique id for the bit\n:param reject_dups: Fail if there is "
          "already a bit in this circuit with the id. Default to True",
          nb::arg("id"), nb::arg("reject_dups") = true)
      .def_prop_ro(
          "qubits", &Circuit::all_qubits,
          "A list of all qubit ids in the circuit")
      .def_prop_ro(
          "created_qubits", &Circuit::created_qubits,
          "A list of qubits whose input is a Create operation")
      .def_prop_ro(
          "discarded_qubits", &Circuit::discarded_qubits,
          "A list of qubits whose output is a Discard operation")
      .def_prop_ro(
          "bits", &Circuit::all_bits,
          "A list of all classical bit ids in the circuit")
      .def_prop_ro(
          "bit_readout", &Circuit::bit_readout,
          "A map from bit to its (left-to-right) index in readouts "
          "from backends (following the increasing lexicographic "
          "order convention)")
      .def_prop_ro(
          "qubit_readout", &Circuit::qubit_readout,
          "A map from qubit to its (left-to-right) index in readouts "
          "from backends. A qubit will feature in this map if it is "
          "measured and neither it nor the bit containing the "
          "measurement result is subsequently acted on")
      .def_prop_ro(
          "qubit_to_bit_map", &Circuit::qubit_to_bit_map,
          "A map from qubit to the bit it is measured to. "
          "A qubit will feature in this map if it is "
          "measured and neither it nor the bit containing the "
          "measurement result is subsequently acted on")
      .def_prop_ro(
          "opgroups", &Circuit::get_opgroups,
          "A set of all opgroup names in the circuit")
      .def(
          "flatten_registers", &Circuit::flatten_registers,
          "Combines all qubits into a single register namespace with "
          "the default name, and likewise for bits")

      // Circuit composition:
      .def(
          "add_circuit",
          [](Circuit &circ, const Circuit &circ2,
             const nb::tket_custom::SequenceVec<Qubit> &qbs,
             const nb::tket_custom::SequenceVec<Bit> &bits) {
            unit_map_t umap;
            unsigned i = 0;
            for (const Qubit &q : qbs) {
              umap.insert({Qubit(i), q});
              ++i;
            }
            i = 0;
            for (const Bit &b : bits) {
              umap.insert({Bit(i), b});
              ++i;
            }
            circ.append_with_map(circ2, umap);
            return &circ;
          },
          "In-place sequential composition of circuits, appending a "
          "copy of the argument onto the end of the circuit. "
          "Connects qubits and bits with the same behaviour as "
          ":py:meth:`add_gate`."
          "\n\n:param circuit: The circuit to be appended to the end "
          "of `self`"
          "\n:param qubits: List mapping the (default register) "
          "qubits of `circuit` to the qubits of `self`"
          "\n:param bits: List mapping the (default register) bits "
          "of `circuit` to the bits of `self`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("circuit"), nb::arg("qubits"), nb::arg("bits") = no_bits)
      .def(
          "add_circuit",
          [](Circuit &circ, const Circuit &circ2,
             const nb::tket_custom::SequenceVec<unsigned> &qbs,
             const nb::tket_custom::SequenceVec<unsigned> &bits) {
            circ.append_qubits(circ2, qbs, bits);
            return &circ;
          },
          "In-place sequential composition of circuits, appending a "
          "copy of the argument onto the end of the circuit. "
          "Connects qubits and bits with the same behaviour as "
          ":py:meth:`add_gate`."
          "\n\n:param circuit: The circuit to be appended to the end "
          "of `self`"
          "\n:param qubits: List mapping the (default register) "
          "qubits of `circuit` to the (default register) qubits of "
          "`self`"
          "\n:param bits: List mapping the (default register) bits "
          "of `circuit` to the (default register) bits of `self`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("circuit"), nb::arg("qubits"), nb::arg("bits") = no_bits)
      .def(
          "add_circuit_with_map",
          [](Circuit &circ, const Circuit &circ2, const unit_map_t &unit_map) {
            circ.append_with_map(circ2, unit_map);
            return &circ;
          },
          "In-place sequential composition of circuits, appending a copy of "
          "the argument onto the end of the circuit."
          "\n\n:param circuit: circuit to be appended"
          "\n:param unit_map: map from qubits and bits in the appended circuit "
          "to corresponding qubits and bits in `self`",
          nb::arg("circuit"), nb::arg("unit_map"))
      .def(
          "append", (void (Circuit::*)(const Circuit &))&Circuit::append,
          "In-place sequential composition of circuits, appending a "
          "copy of the argument onto the end of the circuit. Inputs and "
          "Outputs are unified if they share the same id, defaulting to "
          "parallel composition if there is no match."
          "\n\n:param circuit: The circuit to be appended to the end of "
          "`self`",
          nb::arg("circuit"))
      .def(
          "add_phase",
          [](Circuit &circ, const Expr &a) {
            circ.add_phase(a);
            return &circ;
          },
          "Add a global phase to the circuit.\n\n:param a: Phase to "
          "add, in halfturns\n\n:return: circuit with added phase",
          nb::arg("a"))
      .def(
          "_n_vertices", &Circuit::n_vertices,
          ":return: the number of vertices in the DAG, i.e. the sum of "
          "the number of operations, inputs, and outputs")
      .def_prop_ro(
          "is_simple", [](const Circuit &circ) { return circ.is_simple(); },
          "Checks that the circuit has only 1 quantum and 1 classic "
          "register using the default names ('q' and 'c'). This "
          "means it is suitable to refer to qubits simply by their "
          "integer indices.")
      .def_prop_ro(
          "n_gates", &Circuit::n_gates, "The number of gates in the Circuit")
      .def_prop_ro(
          "wasm_uid",
          [](const Circuit &circ) { return circ.get_wasm_file_uid(); },
          "The unique WASM UID of the circuit, or `None` if the "
          "circuit has none")
      .def_prop_ro(
          "n_qubits", &Circuit::n_qubits, "The number of qubits in the circuit")
      .def_prop_ro(
          "n_bits", &Circuit::n_bits,
          "The number of classiclal bits in the circuit")
      .def_prop_ro(
          "phase", &Circuit::get_phase,
          "The global phase applied to the circuit, in "
          "halfturns (not meaningful for circuits with classical "
          "interactions)")
      .def_prop_rw("name", &Circuit::get_name, &Circuit::set_name)
      .def(
          "remove_blank_wires", &Circuit::remove_blank_wires,
          "Removes any Input-Output pairs in the DAG with no "
          "intervening operations, i.e. removes untouched qubits/bits "
          "from the circuit. This may occur when optimisations "
          "recognise that the operations on a qubit reduce to the "
          "identity, or when routing adds wires to \"fill out\" the "
          "architecture. This operation will only remove empty classical wires "
          "if there are no used bits with a higher index in the same register. "
          "\n\n:param keep_blank_classical_wires: if true, empty classical "
          "wires are not removed"
          "\n:param remove_classical_only_at_end_of_register: if true, empty "
          "classical wires are not removed if they don't belong to a register "
          "or there are any non-empty wires with a higher index in the same "
          "register",
          nb::arg("keep_blank_classical_wires") = false,
          nb::arg("remove_classical_only_at_end_of_register") = true)
      .def(
          "add_blank_wires", &Circuit::add_blank_wires,
          "Adds a number of new qubits to the circuit. These will be "
          "added to the default register ('q') if possible, filling out "
          "the unused indices from 0.\n\n:param number: Number of "
          "qubits to add",
          nb::arg("number"))
      .def(
          "rename_units",
          [](Circuit &self, const std::map<PyUnitID, PyUnitID> &py_map) {
            std::map<UnitID, UnitID> cpp_map;
            for (const auto &pair : py_map) {
              cpp_map[to_cpp_unitid(pair.first)] = to_cpp_unitid(pair.second);
            }
            return self.rename_units(cpp_map);
          },
          "Rename qubits and bits simultaneously according to the map "
          "of ids provided\n\n:param map: Dictionary from current ids "
          "to new ids",
          nb::arg("map"))
      .def(
          "depth", &Circuit::depth,
          // for some reason, each c.depth() in this docstring causes stubgen to
          // create a faulty stub these are manually removed within the stub
          // generation script, I couldn't figure out how to do it otherwise
          // without removing the examples
          "Returns the number of interior vertices on the longest path through "
          "the DAG, excluding vertices representing barrier operations."
          "\n\n>>> c = Circuit(3)"
          "\n>>> c.depth()"
          "\n0"
          "\n>>> c.CX(0,1)"
          "\n>>> c.CX(1,2)"
          "\n>>> c.CX(2,0)"
          "\n>>> c.depth()"
          "\n3"
          "\n\n:return: the circuit depth")
      .def(
          "n_gates_of_type",
          [](const Circuit &circ, const OpType &_type,
             const bool include_conditional) {
            return circ.count_gates(_type, include_conditional);
          },
          "Returns the number of vertices in the dag of a given "
          "operation type.\n\n>>> c.CX(0,1)\n>>> c.H(0)\n>>> "
          "c.CX(0,1)\n>>> c.n_gates_of_type(OpType.CX)\n2\n\n:param "
          "type: The operation type to search for\n\n:param "
          "include_conditional: if set to true, conditional gates will "
          "be counted, too\n\n:return: the number of operations "
          "matching `type`",
          nb::arg("type"), nb::arg("include_conditional") = false)
      .def(
          "n_1qb_gates",
          [](const Circuit &circ) { return circ.count_n_qubit_gates(1); },
          "Returns the number of vertices in the dag with one quantum edge."
          "Ignores Input, Create, Output, Discard, Reset, Measure and Barrier "
          "vertices.")
      .def(
          "n_2qb_gates",
          [](const Circuit &circ) { return circ.count_n_qubit_gates(2); },
          "Returns the number of vertices in the dag with two quantum edges."
          "Ignores Input, Create, Output, Discard, Reset, Measure and Barrier "
          "vertices.")
      .def(
          "n_nqb_gates", &Circuit::count_n_qubit_gates,
          "Returns the number of vertices in the dag with given number of  "
          "quantum edges."
          "Ignores Input, Create, Output, Discard, Reset, Measure and Barrier "
          "vertices.",
          nb::arg("size"))
      .def(
          "depth_by_type", &Circuit::depth_by_type,
          "Returns the number of vertices in the longest path through the "
          "sub-DAG consisting of vertices representing operations of the given "
          "type."
          "\n\n>>> c = Circuit(3)"
          "\n>>> c.CX(0,1)"
          "\n>>> c.Z(1)"
          "\n>>> c.CX(1,2)"
          "\n>>> c.depth_by_type(OpType.CX)"
          "\n2"
          "\n\n:param type: the operation type of interest"
          "\n:return: the circuit depth with respect to operations matching "
          "`type`",
          nb::arg("type"))
      .def(
          "depth_by_type", &Circuit::depth_by_types,
          "Returns the number of vertices in the longest path through the "
          "sub-DAG consisting of vertices representing operations of the given "
          "types."
          "\n\n>>> c = Circuit(3)"
          "\n>>> c.CZ(0,1)"
          "\n>>> c.Z(1)"
          "\n>>> c.CX(1,2)"
          "\n>>> c.depth_by_type({OpType.CZ, OpType.CX})"
          "\n2"
          "\n\n:param types: the set of operation types of interest"
          "\n:return: the circuit depth with respect to operations matching an "
          "element of `types`",
          nb::arg("types"))
      .def(
          "depth_2q", &Circuit::depth_2q,
          // for some reason, each c.depth_2q() in this docstring causes stubgen
          // to create a faulty stub these are manually removed within the stub
          // generation script, I couldn't figure out how to do it otherwise
          // without removing the examples
          "Returns the number of vertices in the longest path through the "
          "sub-DAG consisting of vertices with 2 quantum wires,"
          "excluding vertices representing barrier operations."
          "\n\n>>> c = Circuit(3)"
          "\n>>> c.CZ(0,1)"
          "\n>>> c.Z(0)"
          "\n>>> c.Z(1)"
          "\n>>> c.ZZMax(1,2)"
          "\n>>> c.CX(1,2)"
          "\n>>> c.depth_2q()"
          "\n3"
          "\n:return: the circuit depth with respect to 2-qubit operations.")
      .def(
          "_to_graphviz_file", &Circuit::to_graphviz_file,
          "Saves a visualisation of a circuit's DAG to a \".dot\" file",
          nb::arg("filename"))
      .def(
          "to_dict",
          [](const Circuit &c) {
            return nb::cast<nb::dict>(nb::object(json(c)));
          },
          ":return: a JSON serializable dictionary representation of "
          "the Circuit")
      .def(
          "to_json", [](const Circuit &c) { return json(c).dump(); },
          ":return: a JSON serialized UTF-8 string representation of "
          "the Circuit")
      .def_static(
          "from_dict",
          [](const nb::dict &circuit_dict) {
            return json(circuit_dict).get<Circuit>();
          },
          "Construct Circuit instance from JSON serializable "
          "dictionary representation of the Circuit.")
      .def_static(
          "from_json",
          [](const std::string &circuit_str) {
            Circuit c = json::parse(circuit_str);
            return c;
          },
          "Construct Circuit instance from JSON serialized "
          "UTF-8 string representation of the Circuit.")
      .def(
          "__getstate__",
          [](const Circuit &circ) {
            return nb::make_tuple(nb::cast<nb::dict>(nb::object(json(circ))));
          })
      .def(
          "__setstate__",
          [](Circuit &circ, const nb::tuple &t) {
            const json j = nb::cast<nb::dict>(t[0]);
            new (&circ) Circuit(j.get<Circuit>());
          })
      .def(
          "to_latex_file", &Circuit::to_latex_file,
          "Produces a latex file with a visualisation of the circuit "
          "using the Quantikz package.\n\n:param filename: Name of file "
          "to write output to (must end in \".tex\")",
          nb::arg("filename"))
      .def(
          nb::self >> nb::self,
          "Creates a new Circuit, corresponding to the sequential "
          "composition of the given Circuits. Any qubits/bits with the "
          "same ids will be unified. Any ids without a match will be "
          "added in parallel.")
      .def(
          nb::self * nb::self,
          "Creates a new Circuit, corresponding to the parallel "
          "composition of the given Circuits. This will fail if the "
          "circuits share qubits/bits with the same ids.")
      .def(
          "dagger", &Circuit::dagger,
          "Given a pure circuit (i.e. without any measurements or "
          "conditional gates), produces a new circuit for the "
          "inverse/adjoint operation.\n\n:return: a new "
          ":py:class:`~.Circuit` corresponding to the inverse operation")
      .def(
          "transpose", &Circuit::transpose,
          "Given a pure circuit (i.e. without any measurements or "
          "conditional gates), produces a new circuit for the "
          "transpose operation.\n\n:return: a new "
          ":py:class:`~.Circuit` corresponding to the transpose operation")
      .def(
          "copy", [](Circuit &circ) { return Circuit(circ); },
          ":return: an identical copy of the circuit")
      .def(
          "symbol_substitution",
          (void (Circuit::*)(
              const symbol_map_t &))&Circuit::symbol_substitution,
          "In-place substitution for symbolic expressions; iterates "
          "through each parameterised gate/box and performs the "
          "substitution. \n\n:param symbol_map: A map from "
          "SymPy symbols to SymPy expressions or floats",
          nb::arg("symbol_map"))
      .def(
          "free_symbols", &Circuit::free_symbols,
          ":return: set of symbolic parameters in the circuit")
      .def(
          "is_symbolic", &Circuit::is_symbolic,
          ":return: True if the circuit "
          "contains any free symbols, False otherwise.")
      .def(
          "substitute_named",
          [](Circuit &circ, Op_ptr op, const std::string &opgroup) {
            return circ.substitute_named(std::move(op), opgroup);
          },
          "Substitute all ops with the given name for the given op."
          "The replacement operations retain the same name.\n\n"
          ":param op: the replacement operation\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("op"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const Circuit &repl, const std::string &opgroup) {
            return circ.substitute_named(repl, opgroup);
          },
          "Substitute all ops with the given name for the given circuit."
          "Named operations in the replacement circuit must not match "
          "any named operations in the circuit being modified.\n\n"
          ":param repl: the replacement circuit\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("repl"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const CircBox &box, const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement CircBox\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const Unitary1qBox &box,
             const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement Unitary1qBox\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const Unitary2qBox &box,
             const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement Unitary2qBox\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const Unitary3qBox &box,
             const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement Unitary3qBox\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const ExpBox &box, const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement ExpBox\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const PauliExpBox &box,
             const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement PauliExpBox\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const ToffoliBox &box, const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement ToffoliBox\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const DummyBox &box, const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement DummyBox\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const QControlBox &box,
             const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement QControlBox\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))
      .def(
          "substitute_named",
          [](Circuit &circ, const CustomGate &box, const std::string &opgroup) {
            return circ.substitute_named(box, opgroup);
          },
          "Substitute all ops with the given name for the given box."
          "The replacement boxes retain the same name.\n\n"
          ":param box: the replacement CustomGate\n"
          ":param opgroup: the name of the operations group to replace\n"
          ":return: whether any replacements were made",
          nb::arg("box"), nb::arg("opgroup"))

      // Methods for contextual optimization
      .def(
          "qubit_create", &Circuit::qubit_create,
          "Make a quantum input a Create operation (initialized to 0)")
      .def(
          "qubit_discard", &Circuit::qubit_discard,
          "Make a quantum output a Discard operation")
      .def(
          "qubit_create_all", &Circuit::qubit_create_all,
          "Make all quantum inputs Create operations (initialized to 0)")
      .def(
          "qubit_discard_all", &Circuit::qubit_discard_all,
          "Make all quantum outputs Discard operations")
      .def(
          "qubit_is_created", &Circuit::is_created,
          "Query whether a qubit has its initial state set to zero")
      .def(
          "qubit_is_discarded", &Circuit::is_discarded,
          "Query whether a qubit has its final state discarded")
      .def("_classical_eval", &Circuit::classical_eval)

      .def(
          "valid_connectivity", &respects_connectivity_constraints,
          "Confirms whether all two qubit gates in given circuit are "
          "along some edge of the architecture."
          "\n\n:param arch: The architecture capturing the desired "
          "connectivity"
          "\n:param directed: If true, also checks that CX or ECR gates are in "
          "the same direction as the edges of the architecture"
          "\n:param allow_bridge: Accept BRIDGEs as valid, assuming the "
          "middle qubit neighbours the others"
          "\n\n:return: True or False",
          nb::arg("arch"), nb::arg("directed"), nb::arg("allow_bridge") = false)
      .def(
          "implicit_qubit_permutation", &Circuit::implicit_qubit_permutation,
          ":return: dictionary mapping input qubit to output qubit on "
          "the same path")
      .def_prop_ro(
          "has_implicit_wireswaps", &Circuit::has_implicit_wireswaps,
          "Indicates whether the circuit has a non-trivial qubit permutation "
          "(i.e., whether there are any implicit wire swaps).")
      .def(
          "replace_SWAPs", &Circuit::replace_SWAPs,
          "Replace all SWAP gates with implicit wire swaps."
          "\n\n:param replace_tk2_equivalents: If True, TK2 gates that are "
          "equivalent to SWAPs will also be replaced. Defaults to False."
          "\n\n:return: whether any SWAP gates were replaced",
          nb::arg("replace_tk2_equivalents") = false)
      .def(
          "replace_implicit_wire_swaps",
          &Circuit::replace_all_implicit_wire_swaps,
          "Replace all implicit wire swaps with SWAP gates.")
      .def(
          "ops_of_type",
          [](const Circuit &circ, OpType optype) {
            VertexSet vset = circ.get_gates_of_type(optype);
            std::list<Op_ptr> ops;
            for (const auto &v : vset) {
              ops.push_back(circ.dag[v].op);
            }
            return ops;
          },
          "Get all operations in the circuit of a given type."
          "\n\nThe order is not guaranteed."
          "\n\n:param optype: operation type"
          "\n\n:return: list of :py:class:`~.Op`",
          nb::arg("optype"))
      .def(
          "commands_of_type", &Circuit::get_commands_of_type,
          "Get all commands in a circuit of a given type."
          "\n\nThe order is consistent with the causal order of the "
          "operations in the circuit."
          "\n\n:param optype: operation type"
          "\n\n:return: list of :py:class:`~.Command`",
          nb::arg("optype"))
      .def(
          "get_resources", &Circuit::get_resources,
          "Calculate the overall resources of the circuit."
          "\n\nThis takes account of the data stored in each "
          "py:class:`DummyBox` within the circuit, as well as other gates, "
          "to compute upper and lower bounds."
          "\n\n:return: bounds on resources of the circuit"
          "\n\n"
          ">>> resource_data0 = ResourceData(\n"
          "...     op_type_count={\n"
          "...         OpType.T: ResourceBounds(1, 2),\n"
          "...         OpType.H: ResourceBounds(0, 1),\n"
          "...         OpType.CX: ResourceBounds(1, 2),\n"
          "...         OpType.CZ: ResourceBounds(3, 3),\n"
          "...     },\n"
          "...     gate_depth=ResourceBounds(5, 8),\n"
          "...     op_type_depth={\n"
          "...         OpType.T: ResourceBounds(0, 10),\n"
          "...         OpType.H: ResourceBounds(0, 10),\n"
          "...         OpType.CX: ResourceBounds(1, 2),\n"
          "...         OpType.CZ: ResourceBounds(3, 3),\n"
          "...     },\n"
          "...     two_qubit_gate_depth=ResourceBounds(4, 5),\n"
          "... )\n"
          ">>> dbox0 = DummyBox(n_qubits=2, n_bits=0, "
          "resource_data=resource_data0)\n"
          ">>> resource_data1 = ResourceData(\n"
          "...     op_type_count={\n"
          "...         OpType.T: ResourceBounds(2, 2),\n"
          "...         OpType.H: ResourceBounds(1, 1),\n"
          "...         OpType.CX: ResourceBounds(2, 3),\n"
          "...         OpType.CZ: ResourceBounds(3, 5),\n"
          "...     },\n"
          "...     gate_depth=ResourceBounds(5, 10),\n"
          "...     op_type_depth={\n"
          "...         OpType.T: ResourceBounds(1, 2),\n"
          "...         OpType.H: ResourceBounds(2, 4),\n"
          "...         OpType.CX: ResourceBounds(1, 1),\n"
          "...         OpType.CZ: ResourceBounds(3, 4),\n"
          "...     },\n"
          "...     two_qubit_gate_depth=ResourceBounds(3, 5),\n"
          "... )\n"
          ">>> dbox1 = DummyBox(n_qubits=3, n_bits=0, "
          "resource_data=resource_data1)\n"
          ">>> c = (\n"
          "...     Circuit(3)\n"
          "...     .H(0)\n"
          "...     .CX(1, 2)\n"
          "...     .CX(0, 1)\n"
          "...     .T(2)\n"
          "...     .H(1)\n"
          "...     .add_dummybox(dbox0, [0, 1], [])\n"
          "...     .CZ(1, 2)\n"
          "...     .add_dummybox(dbox1, [0, 1, 2], [])\n"
          "...     .H(2)\n"
          "... )\n"
          ">>> resource_data = c.get_resources()\n"
          ">>> print(resource_data)\n"
          "ResourceData(op_type_count={OpType.T: ResourceBounds(4, 5), "
          "OpType.H: ResourceBounds(4, 5), OpType.CX: ResourceBounds(5, 7), "
          "OpType.CZ: ResourceBounds(7, 9), }, gate_depth=ResourceBounds(15, "
          "23), op_type_depth={OpType.T: ResourceBounds(2, 12), OpType.H: "
          "ResourceBounds(5, 17), OpType.CX: ResourceBounds(4, 5), OpType.CZ: "
          "ResourceBounds(7, 8), }, two_qubit_gate_depth=ResourceBounds(10, "
          "13))")
      .def_prop_ro(
          "_dag_data",
          [](Circuit &circ) {
            IndexMap im = circ.index_map();

            // subset of quantum input nodes
            std::set<unsigned> q_inputs;
            for (const Vertex &v : circ.q_inputs()) {
              q_inputs.insert(im[v]);
            }

            // subset of classical input nodes
            std::set<unsigned> c_inputs;
            for (const Vertex &v : circ.c_inputs()) {
              c_inputs.insert(im[v]);
            }

            // subset of wasm input nodes
            std::set<unsigned> w_inputs;
            for (const Vertex &v : circ.w_inputs()) {
              w_inputs.insert(im[v]);
            }

            // subset of RNG input nodes
            std::set<unsigned> r_inputs;
            for (const Vertex &v : circ.r_inputs()) {
              r_inputs.insert(im[v]);
            }

            // subset of quantum output nodes
            std::set<unsigned> q_outputs;
            for (const Vertex &v : circ.q_outputs()) {
              q_outputs.insert(im[v]);
            }

            // subset of classical output nodes
            std::set<unsigned> c_outputs;
            for (const Vertex &v : circ.c_outputs()) {
              c_outputs.insert(im[v]);
            }

            // subset of wasm output nodes
            std::set<unsigned> w_outputs;
            for (const Vertex &v : circ.w_outputs()) {
              w_outputs.insert(im[v]);
            }

            // subset of RNG output nodes
            std::set<unsigned> r_outputs;
            for (const Vertex &v : circ.r_outputs()) {
              r_outputs.insert(im[v]);
            }

            // maps from input and output nodes to unit names
            std::map<unsigned, std::string> input_names, output_names;
            for (const BoundaryElement &b : circ.boundary) {
              std::string bname = b.id_.repr();
              input_names[im[b.in_]] = bname;
              output_names[im[b.out_]] = bname;
            }

            // map from node to string description
            std::map<unsigned, std::string> node_data;
            BGL_FORALL_VERTICES(v, circ.dag, DAG) {
              node_data[im[v]] = circ.get_Op_ptr_from_Vertex(v)->get_name();
            }

            // set of tuples (source node, target node, source port, target
            // port, edge type)
            std::set<
                std::tuple<unsigned, unsigned, unsigned, unsigned, std::string>>
                edge_data;
            BGL_FORALL_EDGES(e, circ.dag, DAG) {
              Vertex v_so = circ.source(e);
              Vertex v_ta = circ.target(e);
              unsigned v_s = im[v_so];
              unsigned v_t = im[v_ta];
              EdgeType edge_type = circ.dag[e].type;
              // This transformation is necessary due to a bug encountered
              // with pybind11 on mac0S 11 only, that causes the python sided
              // Enum class to take nonsensical values
              std::string edge_type_str =
                  (edge_type == EdgeType::Quantum)     ? "Quantum"
                  : (edge_type == EdgeType::Boolean)   ? "Boolean"
                  : (edge_type == EdgeType::Classical) ? "Classical"
                                                       : "WASM";
              edge_data.insert(
                  {v_s, v_t, circ.get_source_port(e), circ.get_target_port(e),
                   edge_type_str});
            }

            return std::make_tuple(
                q_inputs, c_inputs, w_inputs, r_inputs, q_outputs, c_outputs,
                w_outputs, r_outputs, input_names, output_names, node_data,
                edge_data);
          },
          "DAG data for circuit");
}

}  // namespace tket

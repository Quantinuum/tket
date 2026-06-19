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
#include <nanobind/eigen/sparse.h>
#include <nanobind/operators.h>

#include "deleted_hash.hpp"
#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"
#include "py_operators.hpp"
#include "tket/Utils/PauliTensor.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

typedef nb::tket_custom::SequenceVec<Qubit> py_qubit_vector_t;
NB_MODULE(pauli, m) {
  nb::set_leak_warnings(false);
  nb::module_::import_("pytket._tket.unit_id");
  nb::enum_<Pauli>(m, "Pauli")
      .value("I", Pauli::I)
      .value("X", Pauli::X)
      .value("Y", Pauli::Y)
      .value("Z", Pauli::Z)
      .export_values();

  nb::class_<SpPauliString>(
      m, "QubitPauliString",
      "A string of Pauli letters from the alphabet {I, X, Y, Z}, "
      "implemented as a sparse list, indexed by qubit.")
      .def(nb::init<>(), "Constructs an empty QubitPauliString.")
      .def(
          nb::init<Qubit, Pauli>(),
          "Constructs a QubitPauliString with a single Pauli term.",
          nb::arg("qubit"), nb::arg("pauli"))
      .def(
          nb::init<
              nb::tket_custom::SequenceList<Qubit>,
              nb::tket_custom::SequenceList<Pauli>>(),
          "Constructs a QubitPauliString from two matching lists of "
          "Qubits and Paulis.",
          nb::arg("qubits"), nb::arg("paulis"))
      .def(
          nb::init<QubitPauliMap>(),
          "Construct a QubitPauliString from a dictionary mapping "
          ":py:class:`~.Qubit` to :py:class:`~.Pauli`.",
          nb::arg("map"))
      .def(
          "__hash__", [](const SpPauliString &qps) { return qps.hash_value(); })
      .def("__repr__", &SpPauliString::to_str)
      .def("__eq__", &py_equals<SpPauliString>)
      .def("__ne__", &py_not_equals<SpPauliString>)
      .def("__lt__", &SpPauliString::operator<)
      .def("__getitem__", &SpPauliString::get<QubitPauliMap>)
      .def("__setitem__", &SpPauliString::set<QubitPauliMap>)
      .def_prop_ro(
          "map", [](const SpPauliString &qps) { return qps.string; },
          "The QubitPauliString's underlying dict mapping "
          ":py:class:`~.Qubit` to :py:class:`~.Pauli`")
      .def(
          "to_list",
          [](const SpPauliString &qps) {
            // Just return the QubitPauliMap for backwards compatibility with
            // before templated PauliTensor
            return nb::cast<nb::list>(nb::object(json(qps.string)));
          },
          "A JSON-serializable representation of the QubitPauliString.\n\n"
          ":return: a list of :py:class:`~.Qubit`-to-:py:class:`~.Pauli` "
          "entries, "
          "represented as dicts.")
      .def_static(
          "from_list",
          [](const nb::list &qubit_pauli_string_list) {
            return SpPauliString(
                json(qubit_pauli_string_list).get<QubitPauliMap>());
          },
          "Construct a new QubitPauliString instance from a JSON serializable "
          "list "
          "representation.")
      .def(
          "compress", &SpPauliString::compress<QubitPauliMap>,
          "Removes I terms to compress the sparse representation.")
      .def(
          "commutes_with", &SpPauliString::commutes_with<no_coeff_t>,
          ":return: True if the two strings commute, else False",
          nb::arg("other"))
      .def(
          "to_sparse_matrix",
          (CmplxSpMat (SpPauliString::*)(void) const) &
              SpPauliString::to_sparse_matrix,
          "Represents the sparse string as a dense string (without "
          "padding for extra qubits) and generates the matrix for the "
          "tensor. Uses the ILO-BE convention, so ``Qubit(\"a\", 0)`` "
          "is more significant that ``Qubit(\"a\", 1)`` and "
          "``Qubit(\"b\")`` for indexing into the matrix."
          "\n\n:return: a sparse matrix corresponding to the operator")
      .def(
          "to_sparse_matrix",
          (CmplxSpMat (SpPauliString::*)(const unsigned) const) &
              SpPauliString::to_sparse_matrix,
          "Represents the sparse string as a dense string over "
          "`n_qubits` qubits (sequentially indexed from 0 in the "
          "default register) and generates the matrix for the tensor. "
          "Uses the ILO-BE convention, so ``Qubit(0)`` is the most "
          "significant bit for indexing into the matrix."
          "\n\n:param n_qubits: the number of qubits in the full "
          "operator"
          "\n:return: a sparse matrix corresponding to the operator",
          nb::arg("n_qubits"))
      .def(
          "to_sparse_matrix",
          [](const SpPauliString &self, const py_qubit_vector_t &qubits) {
            return self.to_sparse_matrix(qubits);
          },
          "Represents the sparse string as a dense string and generates "
          "the matrix for the tensor. Orders qubits according to "
          "`qubits` (padding with identities if they are not in the "
          "sparse string), so ``qubits[0]`` is the most significant bit "
          "for indexing into the matrix."
          "\n\n:param qubits: the ordered list of qubits in the full "
          "operator"
          "\n:return: a sparse matrix corresponding to the operator",
          nb::arg("qubits"))
      .def(
          "dot_state",
          (Eigen::VectorXcd (SpPauliString::*)(const Eigen::VectorXcd &)
               const) &
              SpPauliString::dot_state,
          "Performs the dot product of the state with the pauli string. "
          "Maps the qubits of the statevector with sequentially-indexed "
          "qubits in the default register, with ``Qubit(0)`` being the "
          "most significant qubit."
          "\n\n:param state: statevector for qubits ``Qubit(0)`` to "
          "``Qubit(n-1)``"
          "\n:return: dot product of operator with state",
          nb::arg("state"))
      .def(
          "dot_state",
          [](const SpPauliString &self, const Eigen::VectorXcd &state,
             const py_qubit_vector_t &qubits) {
            return self.dot_state(state, qubits);
          },
          "Performs the dot product of the state with the pauli string. "
          "Maps the qubits of the statevector according to the ordered "
          "list `qubits`, with ``qubits[0]`` being the most significant "
          "qubit."
          "\n\n:param state: statevector"
          "\n:param qubits: order of qubits in `state` from most to "
          "least significant"
          "\n:return: dot product of operator with state",
          nb::arg("state"), nb::arg("qubits"))
      .def(
          "state_expectation",
          [](const SpPauliString &self, const Eigen::VectorXcd &state) {
            return self.state_expectation(state);
          },
          "Calculates the expectation value of the state with the pauli "
          "string. Maps the qubits of the statevector with "
          "sequentially-indexed qubits in the default register, with "
          "``Qubit(0)`` being the most significant qubit."
          "\n\n:param state: statevector for qubits ``Qubit(0)`` to "
          "``Qubit(n-1)``"
          "\n:return: expectation value with respect to state",
          nb::arg("state"))
      .def(
          "state_expectation",
          [](const SpPauliString &self, const Eigen::VectorXcd &state,
             const py_qubit_vector_t &qubits) {
            return self.state_expectation(state, qubits);
          },
          "Calculates the expectation value of the state with the pauli "
          "string. Maps the qubits of the statevector according to the "
          "ordered list `qubits`, with ``qubits[0]`` being the most "
          "significant qubit."
          "\n\n:param state: statevector"
          "\n:param qubits: order of qubits in `state` from most to "
          "least significant"
          "\n:return: expectation value with respect to state",
          nb::arg("state"), nb::arg("qubits"))
      .def(
          "__getstate__",
          [](const SpPauliString &qps) {
            /* Hackery to avoid pickling an opaque object */
            std::list<Qubit> qubits;
            std::list<Pauli> paulis;
            for (const std::pair<const Qubit, Pauli> &qp_pair : qps.string) {
              qubits.push_back(qp_pair.first);
              paulis.push_back(qp_pair.second);
            }
            return nb::make_tuple(qubits, paulis);
          })
      .def("__setstate__", [](SpPauliString &qps, const nb::tuple &t) {
        if (t.size() != 2) {
          throw std::runtime_error(
              "Invalid state: tuple size: " + std::to_string(t.size()));
        }
        new (&qps) SpPauliString(
            nb::cast<std::list<Qubit>>(t[0]), nb::cast<std::list<Pauli>>(t[1]));
      });
  m.def(
      "pauli_string_mult",
      [](const SpPauliString &qps1, const SpPauliString &qps2) {
        SpCxPauliTensor product_tensor =
            SpCxPauliTensor(qps1) * SpCxPauliTensor(qps2);
        return std::pair<SpPauliString, Complex>(
            product_tensor.string, product_tensor.coeff);
      },
      ":return: the product of two QubitPauliString objects as a pair "
      "(QubitPauliString, complex)",
      nb::arg("qubitpaulistring1"), nb::arg("qubitpaulistring2"));

  nb::class_<PauliStabiliser>(
      m, "PauliStabiliser",
      "A string of Pauli letters from the alphabet {I, X, Y, Z} "
      "with a +/- 1 coefficient.")
      .def(nb::init<>(), "Constructs an empty PauliStabiliser.")
      .def(
          "__init__",
          [](PauliStabiliser *p,
             const nb::tket_custom::SequenceVec<Pauli> &string,
             const int &coeff) {
            int half_pis = 1 - coeff;
            if (half_pis != 0 && half_pis != 2) {
              throw std::invalid_argument("Coefficient must be -1 or 1.");
            }
            new (p) PauliStabiliser(string, half_pis);
          },
          "Constructs a PauliStabiliser with a list of Pauli terms.",
          nb::arg("string"), nb::arg("coeff"))
      .def_prop_ro(
          "coeff",
          [](const PauliStabiliser &stabiliser) {
            return stabiliser.is_real_negative() ? -1 : 1;
          },
          "The coefficient of the stabiliser")
      .def_prop_ro(
          "string",
          [](const PauliStabiliser &stabiliser) { return stabiliser.string; },
          "The list of Pauli terms")
      .def("__eq__", &py_equals<PauliStabiliser>)
      .def("__hash__", &deletedHash<PauliStabiliser>, deletedHashDocstring)
      .def("__ne__", &py_not_equals<PauliStabiliser>);

  nb::class_<SpCxPauliTensor>(
      m, "QubitPauliTensor",
      "A tensor formed by Pauli terms, consisting of a sparse map from "
      ":py:class:`~.Qubit` to :py:class:`~.Pauli` (implemented as a "
      ":py:class:`~.QubitPauliString`) and a complex coefficient.")
      .def(
          "__init__",
          [](SpCxPauliTensor *p, const Complex &coeff) {
            new (p) SpCxPauliTensor({}, coeff);
          },
          "Constructs an empty QubitPauliTensor, representing the identity.",
          nb::arg("coeff") = 1.)
      .def(
          nb::init<Qubit, Pauli, Complex>(),
          "Constructs a QubitPauliTensor with a single Pauli term.",
          nb::arg("qubit"), nb::arg("pauli"), nb::arg("coeff") = 1.)
      .def(
          nb::init<
              const nb::tket_custom::SequenceList<Qubit> &,
              const nb::tket_custom::SequenceList<Pauli> &, const Complex &>(),
          "Constructs a QubitPauliTensor from two matching lists of "
          "Qubits and Paulis.",
          nb::arg("qubits"), nb::arg("paulis"), nb::arg("coeff") = 1.)
      .def(
          nb::init<QubitPauliMap, Complex>(),
          "Construct a QubitPauliTensor from a dictionary mapping "
          ":py:class:`~.Qubit` to :py:class:`~.Pauli`.",
          nb::arg("map"), nb::arg("coeff") = 1.)
      .def(
          "__init__",
          [](SpCxPauliTensor *p, const SpPauliString &qps, const Complex &c) {
            new (p) SpCxPauliTensor(qps.string, c);
          },
          "Construct a QubitPauliTensor from a QubitPauliString.",
          nb::arg("string"), nb::arg("coeff") = 1.)
      .def(
          "__hash__",
          [](const SpCxPauliTensor &qps) { return qps.hash_value(); })
      .def("__repr__", &SpCxPauliTensor::to_str)
      .def("__eq__", &py_equals<SpCxPauliTensor>)
      .def("__ne__", &py_not_equals<SpCxPauliTensor>)
      .def("__lt__", &SpCxPauliTensor::operator<)
      .def("__getitem__", &SpCxPauliTensor::get<QubitPauliMap>)
      .def("__setitem__", &SpCxPauliTensor::set<QubitPauliMap>)
      .def(nb::self * nb::self)
      .def(
          "__rmul__",
          [](const SpCxPauliTensor &qpt, const Complex &c) {
            return SpCxPauliTensor(qpt.string, qpt.coeff * c);
          },
          nb::is_operator())
      .def_prop_rw(
          "string",
          [](const SpCxPauliTensor &qpt) {
            // Return as SpPauliString for backwards compatibility with before
            // templated PauliTensor
            return SpPauliString(qpt.string);
          },
          [](SpCxPauliTensor &qpt, const SpPauliString &qps) {
            qpt.string = qps.string;
          },
          "The QubitPauliTensor's underlying :py:class:`~.QubitPauliString`")
      .def_rw(
          "coeff", &SpCxPauliTensor::coeff,
          "The global coefficient of the tensor")
      .def(
          "compress", &SpCxPauliTensor::compress<QubitPauliMap>,
          "Removes I terms to compress the sparse representation.")
      .def(
          "commutes_with", &SpCxPauliTensor::commutes_with<Complex>,
          ":return: True if the two tensors commute, else False",
          nb::arg("other"))
      .def(
          "to_sparse_matrix",
          [](const SpCxPauliTensor &qpt) { return qpt.to_sparse_matrix(); },
          "Represents the sparse string as a dense string (without "
          "padding for extra qubits) and generates the matrix for the "
          "tensor. Uses the ILO-BE convention, so ``Qubit(\"a\", 0)`` "
          "is more significant that ``Qubit(\"a\", 1)`` and "
          "``Qubit(\"b\")`` for indexing into the matrix."
          "\n\n:return: a sparse matrix corresponding to the tensor")
      .def(
          "to_sparse_matrix",
          [](const SpCxPauliTensor &qpt, unsigned n_qubits) {
            return qpt.to_sparse_matrix(n_qubits);
          },
          "Represents the sparse string as a dense string over "
          "`n_qubits` qubits (sequentially indexed from 0 in the "
          "default register) and generates the matrix for the tensor. "
          "Uses the ILO-BE convention, so ``Qubit(0)`` is the most "
          "significant bit for indexing into the matrix."
          "\n\n:param n_qubits: the number of qubits in the full "
          "operator"
          "\n:return: a sparse matrix corresponding to the operator",
          nb::arg("n_qubits"))
      .def(
          "to_sparse_matrix",
          [](const SpCxPauliTensor &qpt, const py_qubit_vector_t &qubits) {
            return qpt.to_sparse_matrix(qubits);
          },
          "Represents the sparse string as a dense string and generates "
          "the matrix for the tensor. Orders qubits according to "
          "`qubits` (padding with identities if they are not in the "
          "sparse string), so ``qubits[0]`` is the most significant bit "
          "for indexing into the matrix."
          "\n\n:param qubits: the ordered list of qubits in the full "
          "operator"
          "\n:return: a sparse matrix corresponding to the operator",
          nb::arg("qubits"))
      .def(
          "dot_state",
          [](const SpCxPauliTensor &qpt, const Eigen::VectorXcd &state) {
            return qpt.dot_state(state);
          },
          "Performs the dot product of the state with the pauli tensor. "
          "Maps the qubits of the statevector with sequentially-indexed "
          "qubits in the default register, with ``Qubit(0)`` being the "
          "most significant qubit."
          "\n\n:param state: statevector for qubits ``Qubit(0)`` to "
          "``Qubit(n-1)``"
          "\n:return: dot product of operator with state",
          nb::arg("state"))
      .def(
          "dot_state",
          [](const SpCxPauliTensor &qpt, const Eigen::VectorXcd &state,
             const py_qubit_vector_t &qubits) {
            return qpt.dot_state(state, qubits);
          },
          "Performs the dot product of the state with the pauli tensor. "
          "Maps the qubits of the statevector according to the ordered "
          "list `qubits`, with ``qubits[0]`` being the most significant "
          "qubit."
          "\n\n:param state: statevector"
          "\n:param qubits: order of qubits in `state` from most to "
          "least significant"
          "\n:return: dot product of operator with state",
          nb::arg("state"), nb::arg("qubits"))
      .def(
          "state_expectation",
          [](const SpCxPauliTensor &qpt, const Eigen::VectorXcd &state) {
            return qpt.state_expectation(state);
          },
          "Calculates the expectation value of the state with the pauli "
          "operator. Maps the qubits of the statevector with "
          "sequentially-indexed qubits in the default register, with "
          "``Qubit(0)`` being the most significant qubit."
          "\n\n:param state: statevector for qubits ``Qubit(0)`` to "
          "``Qubit(n-1)``"
          "\n:return: expectation value with respect to state",
          nb::arg("state"))
      .def(
          "state_expectation",
          [](const SpCxPauliTensor &qpt, const Eigen::VectorXcd &state,
             const py_qubit_vector_t &qubits) {
            return qpt.state_expectation(state, qubits);
          },
          "Calculates the expectation value of the state with the pauli "
          "operator. Maps the qubits of the statevector according to the "
          "ordered list `qubits`, with ``qubits[0]`` being the most "
          "significant qubit."
          "\n\n:param state: statevector"
          "\n:param qubits: order of qubits in `state` from most to "
          "least significant"
          "\n:return: expectation value with respect to state",
          nb::arg("state"), nb::arg("qubits"))
      .def(
          "__getstate__",
          [](const SpCxPauliTensor &qpt) {
            std::list<Qubit> qubits;
            std::list<Pauli> paulis;
            for (const std::pair<const Qubit, Pauli> &qp_pair : qpt.string) {
              qubits.push_back(qp_pair.first);
              paulis.push_back(qp_pair.second);
            }
            return nb::make_tuple(qubits, paulis, qpt.coeff);
          })
      .def("__setstate__", [](SpCxPauliTensor &qpt, const nb::tuple &t) {
        if (t.size() != 3) {
          throw std::runtime_error(
              "Invalid state: tuple size: " + std::to_string(t.size()));
        }
        new (&qpt) SpCxPauliTensor(
            nb::cast<std::list<Qubit>>(t[0]), nb::cast<std::list<Pauli>>(t[1]),
            nb::cast<Complex>(t[2]));
      });
}
}  // namespace tket

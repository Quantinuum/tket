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

#include "tket/Circuit/Boxes.hpp"

#include <nanobind/eigen/dense.h>
#include <nanobind/ndarray.h>

#include <memory>
#include <sstream>
#include <vector>

#include "binder_utils.hpp"
#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/ConjugationBox.hpp"
#include "tket/Circuit/DiagonalBox.hpp"
#include "tket/Circuit/DummyBox.hpp"
#include "tket/Circuit/Multiplexor.hpp"
#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/Circuit/ResourceData.hpp"
#include "tket/Circuit/StatePreparation.hpp"
#include "tket/Circuit/ToffoliBox.hpp"
#include "tket/Converters/PhasePoly.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Utils/HelperFunctions.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

// The typedef PhasePolynomial leads to the python type Dict[List[bool], ...],
// which is not allowed at runtime because lists aren't hashable
typedef nb::tket_custom::SequenceVec<
    std::pair<nb::tket_custom::SequenceVec<bool>, Expr>>
    PyPhasePolynomialAlternate;
PhasePolynomial to_cpp_phase_poly(
    const PyPhasePolynomialAlternate &py_phase_poly) {
  PhasePolynomial phase_poly;
  for (const auto &pair : py_phase_poly) {
    phase_poly.insert_or_assign(pair.first, pair.second);
  }
  return phase_poly;
}
typedef std::map<nb::tket_custom::TupleVec<bool>, Expr> PyPhasePolynomial;
PhasePolynomial to_cpp_phase_poly(const PyPhasePolynomial &py_phase_poly) {
  return PhasePolynomial(
      std::make_move_iterator(py_phase_poly.begin()),
      std::make_move_iterator(py_phase_poly.end()));
}

// state_perm_t has the same hashability problem
typedef nb::tket_custom::SequenceVec<std::pair<
    nb::tket_custom::SequenceVec<bool>, nb::tket_custom::SequenceVec<bool>>>
    py_state_perm_t;
state_perm_t to_cpp_state_perm_t(const py_state_perm_t &py_state_perm) {
  return state_perm_t(
      std::make_move_iterator(py_state_perm.begin()),
      std::make_move_iterator(py_state_perm.end()));
}
typedef std::map<
    nb::tket_custom::TupleVec<bool>, nb::tket_custom::SequenceVec<bool>>
    py_state_perm_t2;
state_perm_t to_cpp_state_perm_t(const py_state_perm_t2 &py_state_perm) {
  return state_perm_t(
      std::make_move_iterator(py_state_perm.begin()),
      std::make_move_iterator(py_state_perm.end()));
}

typedef nb::tket_custom::SequenceVec<
    std::pair<nb::tket_custom::SequenceVec<bool>, Op_ptr>>
    py_ctrl_op_map_t_alt;
ctrl_op_map_t to_cpp_ctrl_op_map_t(const py_ctrl_op_map_t_alt &ctrl_op_list) {
  ctrl_op_map_t ctrl_op_map;
  for (const auto &bit_op_pair : ctrl_op_list) {
    ctrl_op_map.insert_or_assign(bit_op_pair.first, bit_op_pair.second);
  }
  return ctrl_op_map;
}
typedef std::map<nb::tket_custom::TupleVec<bool>, Op_ptr> py_ctrl_op_map_t;
ctrl_op_map_t to_cpp_ctrl_op_map_t(const py_ctrl_op_map_t &ctrl_op_map) {
  return ctrl_op_map_t(
      std::make_move_iterator(ctrl_op_map.begin()),
      std::make_move_iterator(ctrl_op_map.end()));
}

typedef nb::tket_custom::SequenceVec<std::pair<
    nb::tket_custom::SequenceVec<bool>, nb::tket_custom::SequenceVec<Op_ptr>>>
    py_ctrl_tensored_op_map_t_alt;
ctrl_tensored_op_map_t to_cpp_ctrl_op_map_t(
    const py_ctrl_tensored_op_map_t_alt &ctrl_op_list) {
  ctrl_tensored_op_map_t ctrl_op_map;
  for (const auto &bit_op_pair : ctrl_op_list) {
    ctrl_op_map.insert_or_assign(bit_op_pair.first, bit_op_pair.second);
  }
  return ctrl_op_map;
}
typedef std::map<
    nb::tket_custom::TupleVec<bool>, nb::tket_custom::SequenceVec<Op_ptr>>
    py_ctrl_tensored_op_map_t;
ctrl_tensored_op_map_t to_cpp_ctrl_op_map_t(
    const py_ctrl_tensored_op_map_t &ctrl_op_map) {
  return ctrl_tensored_op_map_t(
      std::make_move_iterator(ctrl_op_map.begin()),
      std::make_move_iterator(ctrl_op_map.end()));
}

// Cast the std::vector keys in a map to nb::tuple, since vector is not hashable
// in python
template <class T1, class T2>
std::map<nb::tuple, T2> cast_keys_to_tuples(
    const std::map<std::vector<T1>, T2> &map) {
  std::map<nb::tuple, T2> outmap;
  for (const auto &pair : map) {
    outmap.insert({nb::tuple(nb::cast(pair.first)), pair.second});
  }
  return outmap;
}
// Cast std::map<T1, T2> to std::vector<std::pair<T1, T2>>, useful when T1
// is a vector because vector, i.e. python list, is not hashable in python
template <class T1, class T2>
std::vector<std::pair<T1, T2>> cast_map_to_vector_of_pairs(
    const std::map<T1, T2> &map) {
  std::vector<std::pair<T1, T2>> result;
  result.reserve(map.size());
  for (const auto &pair : map) {
    result.emplace_back(pair.first, pair.second);
  }
  return result;
}

void init_boxes(nb::module_ &m) {
  nb::class_<CircBox, Op>(
      m, "CircBox",
      "A user-defined operation specified by a :py:class:`~.Circuit`.")
      .def(
          nb::init<const Circuit &>(),
          "Construct from a :py:class:`~.Circuit`.", nb::arg("circ"))
      .def(
          "get_circuit", [](CircBox &cbox) { return *cbox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "symbol_substitution",
          [](CircBox &circ, const symbol_map_t &sub_map) {
            circ.symbol_substitution_in_place(sub_map);
          },
          "In-place substitution of symbolic expressions "
          "within underlying circuit; iterates "
          "through each parameterised gate/box within the circuit "
          "and performs the substitution. \n\n WARNING: "
          "This method potentially mutates the CircBox and "
          "any changes are propagated to "
          "any Circuit that the CircBox has been added to "
          "(via Circuit.add_circbox). \n\n:param symbol_map: "
          "A map from SymPy symbols to SymPy expressions",
          nb::arg("symbol_map"))
      .def_prop_rw(
          "circuit_name", &CircBox::get_circuit_name,
          &CircBox::set_circuit_name,
          "The name of the contained circuit. "
          "\n\n WARNING: "
          "Setting this property mutates the CircBox and "
          "any changes are propagated to "
          "any Circuit that the CircBox has been added to "
          "(via Circuit.add_circbox).");
  nb::class_<Unitary1qBox, Op>(
      m, "Unitary1qBox",
      "A user-defined one-qubit operation specified by a unitary matrix.")
      .def(
          nb::init<const Eigen::Matrix2cd &>(),
          "Construct from a unitary matrix.", nb::arg("m"))
      .def(
          "get_circuit", [](Unitary1qBox &ubox) { return *ubox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_matrix", &Unitary1qBox::get_matrix,
          ":return: the unitary matrix as a numpy array");
  nb::class_<Unitary2qBox, Op>(
      m, "Unitary2qBox",
      "A user-defined two-qubit operation specified by a unitary matrix.")
      .def(
          nb::init<const Eigen::Matrix4cd &, BasisOrder>(),
          "Construct from a unitary matrix.\n\n"
          ":param m: The unitary matrix\n"
          ":param basis: Whether the provided unitary is in the ILO-BE "
          "(increasing lexicographic order of qubit ids, big-endian "
          "indexing) format, or DLO-BE (decreasing lexicographic order "
          "of ids)",
          nb::arg("m"), nb::arg("basis") = BasisOrder::ilo)
      .def(
          "get_circuit", [](Unitary2qBox &ubox) { return *ubox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_matrix", &Unitary2qBox::get_matrix,
          ":return: the unitary matrix (in ILO-BE format) as a numpy "
          "array");
  nb::class_<Unitary3qBox, Op>(
      m, "Unitary3qBox",
      "A user-defined three-qubit operation specified by a unitary matrix.")
      .def(
          nb::init<const Eigen::MatrixXcd &, BasisOrder>(),
          "Construct from a unitary matrix.\n\n"
          ":param m: The unitary matrix\n"
          ":param basis: Whether the provided unitary is in the ILO-BE "
          "(increasing lexicographic order of qubit ids, big-endian "
          "indexing) format, or DLO-BE (decreasing lexicographic order "
          "of ids)",
          nb::arg("m"), nb::arg("basis") = BasisOrder::ilo)
      .def(
          "get_circuit", [](Unitary3qBox &ubox) { return *ubox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_matrix", &Unitary3qBox::get_matrix,
          ":return: the unitary matrix (in ILO-BE format) as a numpy array");
  nb::class_<ExpBox, Op>(
      m, "ExpBox",
      "A user-defined two-qubit operation whose corresponding unitary "
      "matrix "
      "is the exponential of a user-defined hermitian matrix.")
      .def(
          nb::init<const Eigen::Matrix4cd &, double, BasisOrder>(),
          "Construct :math:`e^{itA}` from a hermitian matrix :math:`A` "
          "and a parameter :math:`t`.\n\n"
          ":param A: A hermitian matrix\n"
          ":param t: Exponentiation parameter\n"
          ":param basis: Whether the provided matrix is in the ILO-BE "
          "(increasing lexicographic order of qubit ids, big-endian "
          "indexing) format, or DLO-BE (decreasing lexicographic order "
          "of ids)",
          nb::arg("A"), nb::arg("t"), nb::arg("basis") = BasisOrder::ilo)
      .def(
          "get_circuit", [](ExpBox &ebox) { return *ebox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box");
  nb::class_<PauliExpBox, Op>(
      m, "PauliExpBox",
      "An operation defined as the exponential of a tensor of Pauli "
      "operations and a (possibly symbolic) phase parameter.")
      .def(
          "__init__",
          [](PauliExpBox *p, const nb::tket_custom::SequenceVec<Pauli> &paulis,
             Expr t, CXConfigType config) {
            new (p) PauliExpBox(SymPauliTensor(paulis, t), config);
          },
          "Construct :math:`e^{-\\frac12 i \\pi t \\sigma_0 \\otimes "
          "\\sigma_1 \\otimes \\cdots}` from Pauli operators "
          ":math:`\\sigma_i \\in \\{I,X,Y,Z\\}` and a parameter "
          ":math:`t`.",
          nb::arg("paulis"), nb::arg("t"),
          nb::arg("cx_config_type") = CXConfigType::Tree)
      .def(
          "get_circuit", [](PauliExpBox &pbox) { return *pbox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_paulis", &PauliExpBox::get_paulis,
          ":return: the corresponding list of " CLSOBJS(~.Pauli))
      .def(
          "get_phase", &PauliExpBox::get_phase,
          ":return: the corresponding phase parameter")
      .def(
          "get_cx_config", &PauliExpBox::get_cx_config,
          ":return: decomposition method");
  nb::class_<PauliExpPairBox, Op>(
      m, "PauliExpPairBox",
      "A pair of (not necessarily commuting) Pauli exponentials performed in "
      "sequence.\nPairing up exponentials for synthesis can reduce gate costs "
      "of synthesis compared to synthesising individually, with the best "
      "reductions found when the Pauli tensors act on a large number of the "
      "same qubits.\nPhase parameters may be symbolic.")
      .def(
          "__init__",
          [](PauliExpPairBox *p,
             const nb::tket_custom::SequenceVec<Pauli> &paulis0, Expr t0,
             const nb::tket_custom::SequenceVec<Pauli> &paulis1, Expr t1,
             CXConfigType config) {
            new (p) PauliExpPairBox(
                SymPauliTensor(paulis0, t0), SymPauliTensor(paulis1, t1),
                config);
          },
          "Construct a pair of Pauli exponentials of the form"
          " :math:`e^{-\\frac12 i \\pi t_j \\sigma_0 \\otimes "
          "\\sigma_1 \\otimes \\cdots}` from Pauli operator strings "
          ":math:`\\sigma_i \\in \\{I,X,Y,Z\\}` and parameters "
          ":math:`t_j, j \\in \\{0,1\\}`.",
          nb::arg("paulis0"), nb::arg("t0"), nb::arg("paulis1"), nb::arg("t1"),
          nb::arg("cx_config_type") = CXConfigType::Tree)
      .def(
          "get_circuit",
          [](PauliExpPairBox &pbox) { return *pbox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_paulis_pair", &PauliExpPairBox::get_paulis_pair,
          ":return: A tuple containing the two corresponding lists of " CLSOBJS(
              ~.Pauli))
      .def(
          "get_phase_pair", &PauliExpPairBox::get_phase_pair,
          ":return: A tuple containing the two phase parameters")
      .def(
          "get_cx_config", &PauliExpPairBox::get_cx_config,
          ":return: decomposition method");
  nb::class_<PauliExpCommutingSetBox, Op>(
      m, "PauliExpCommutingSetBox",
      "An operation defined as a set of commuting of exponentials of a"
      "tensor of Pauli operations and their (possibly symbolic) phase "
      "parameters.")
      .def(
          "__init__",
          [](PauliExpCommutingSetBox *p,
             const nb::tket_custom::SequenceVec<std::pair<
                 nb::tket_custom::SequenceVec<Pauli>, Expr>> &pauli_gadgets,
             CXConfigType config) {
            std::vector<SymPauliTensor> gadgets;
            for (const std::pair<nb::tket_custom::SequenceVec<Pauli>, Expr> &g :
                 pauli_gadgets)
              gadgets.push_back(SymPauliTensor(g.first, g.second));
            new (p) PauliExpCommutingSetBox(gadgets, config);
          },
          "Construct a set of necessarily commuting Pauli exponentials of the "
          "form"
          " :math:`e^{-\\frac12 i \\pi t_j \\sigma_0 \\otimes "
          "\\sigma_1 \\otimes \\cdots}` from Pauli operator strings "
          ":math:`\\sigma_i \\in \\{I,X,Y,Z\\}` and parameters "
          ":math:`t_j, j \\in \\{0, 1, \\cdots \\}`.",
          nb::arg("pauli_gadgets"),
          nb::arg("cx_config_type") = CXConfigType::Tree)
      .def(
          "get_circuit",
          [](PauliExpCommutingSetBox &pbox) { return *pbox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_paulis",
          [](const PauliExpCommutingSetBox &pbox) {
            // For backwards compatibility with before templated PauliTensor
            std::vector<std::pair<DensePauliMap, Expr>> gadgets;
            for (const SymPauliTensor &g : pbox.get_pauli_gadgets())
              gadgets.push_back({g.string, g.coeff});
            return gadgets;
          },
          ":return: the corresponding list of Pauli gadgets")
      .def(
          "get_cx_config", &PauliExpCommutingSetBox::get_cx_config,
          ":return: decomposition method");

  nb::module_::import_("pytket._tket.transform");
  nb::module_::import_("pytket._tket.partition");
  nb::class_<TermSequenceBox, Op>(
      m, "TermSequenceBox",
      "An unordered collection of Pauli exponentials "
      "that can be synthesised in any order, causing a "
      "change in the unitary operation. Synthesis order "
      "depends on the synthesis strategy chosen only.\n\n WARNING: "
      "Global phase is not preserved when using PauliSynthStrat.Greedy.")
      .def(
          "__init__",
          [](TermSequenceBox *p,
             const nb::tket_custom::SequenceVec<std::pair<
                 nb::tket_custom::SequenceVec<Pauli>, Expr>> &pauli_gadgets,
             Transforms::PauliSynthStrat synth_strat,
             PauliPartitionStrat partition_strat,
             GraphColourMethod graph_colouring, CXConfigType cx_config,
             double depth_weight) {
            std::vector<SymPauliTensor> gadgets;
            for (const std::pair<nb::tket_custom::SequenceVec<Pauli>, Expr> &g :
                 pauli_gadgets)
              gadgets.push_back(SymPauliTensor(g.first, g.second));
            new (p) TermSequenceBox(
                gadgets, synth_strat, partition_strat, graph_colouring,
                cx_config, depth_weight);
          },
          "Construct a set of Pauli exponentials of the "
          "form"
          " :math:`e^{-\\frac12 i \\pi t_j \\sigma_0 \\otimes "
          "\\sigma_1 \\otimes \\cdots}` from Pauli operator strings "
          ":math:`\\sigma_i \\in \\{I,X,Y,Z\\}` and parameters "
          ":math:`t_j, j \\in \\{0, 1, \\cdots \\}`.\n"
          "`depth_weight` controls the degree of depth optimisation and only "
          "applies to synthesis_strategy `PauliSynthStrat:Greedy`. "
          "`partitioning_strategy`, `graph_colouring`, and `cx_config_type` "
          "have no effect if `PauliSynthStrat.Greedy` is used.\n\n WARNING: "
          "Global phase is not preserved when using PauliSynthStrat.Greedy.",
          nb::arg("pauli_gadgets"),
          nb::arg("synthesis_strategy") = Transforms::PauliSynthStrat::Sets,
          nb::arg("partitioning_strategy") = PauliPartitionStrat::CommutingSets,
          nb::arg("graph_colouring") = GraphColourMethod::Lazy,
          nb::arg("cx_config_type") = CXConfigType::Tree,
          nb::arg("depth_weight") = 0.3)
      .def(
          "get_circuit",
          [](TermSequenceBox &pbox) { return *pbox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_paulis",
          [](const TermSequenceBox &pbox) {
            // For backwards compatibility with before templated PauliTensor
            std::vector<std::pair<DensePauliMap, Expr>> gadgets;
            for (const SymPauliTensor &g : pbox.get_pauli_gadgets())
              gadgets.push_back({g.string, g.coeff});
            return gadgets;
          },
          ":return: the corresponding list of Pauli gadgets")
      .def(
          "get_synthesis_strategy", &TermSequenceBox::get_synth_strategy,
          ":return: synthesis strategy")
      .def(
          "get_partition_strategy", &TermSequenceBox::get_partition_strategy,
          ":return: partitioning strategy")
      .def(
          "get_graph_colouring_method", &TermSequenceBox::get_graph_colouring,
          ":return: graph colouring method")
      .def(
          "get_cx_config", &TermSequenceBox::get_cx_config,
          ":return: cx decomposition method")
      .def(
          "get_depth_weight", &TermSequenceBox::get_depth_weight,
          ":return: depth tuning parameter");

  nb::enum_<ToffoliBoxSynthStrat>(
      m, "ToffoliBoxSynthStrat",
      "Enum strategies for synthesising ToffoliBoxes")
      .value(
          "Matching", ToffoliBoxSynthStrat::Matching,
          "Use multiplexors to perform parallel swaps on hypercubes")
      .value(
          "Cycle", ToffoliBoxSynthStrat::Cycle,
          "Use CnX gates to perform transpositions");
  nb::class_<ToffoliBox, Op>(
      m, "ToffoliBox",
      "An operation that constructs a circuit to implement the specified "
      "permutation of classical basis states.")
      .def(
          "__init__",
          [](ToffoliBox *p, const py_state_perm_t &perm,
             ToffoliBoxSynthStrat strat, OpType optype) {
            new (p) ToffoliBox(to_cpp_state_perm_t(perm), strat, optype);
          },
          "Construct from a permutation of basis states\n\n"
          ":param permutation: a list of bitstring pairs\n"
          ":param strat: synthesis strategy\n"
          ":param rotation_axis: the rotation axis of the multiplexors used in "
          "the decomposition. Can be either Rx or Ry. Only applicable to the "
          "Matching strategy. Default to Ry.",
          nb::arg("permutation"), nb::arg("strat"),
          nb::arg("rotation_axis") = OpType::Ry)
      .def(
          "__init__",
          [](ToffoliBox *p, const py_state_perm_t &perm,
             const OpType &rotation_axis) {
            new (p) ToffoliBox(
                to_cpp_state_perm_t(perm), ToffoliBoxSynthStrat::Matching,
                rotation_axis);
          },
          "Construct from a permutation of basis states and perform synthesis "
          "using the Matching strategy\n\n"
          ":param permutation: a list of bitstring pairs\n"
          ":param rotation_axis: the rotation axis of the multiplexors used in "
          "the decomposition. Can be either Rx or Ry, default to Ry.",
          nb::arg("permutation"), nb::arg("rotation_axis") = OpType::Ry)
      .def(
          "__init__",
          [](ToffoliBox *p, unsigned n_qubits, const py_state_perm_t &perm,
             const OpType &rotation_axis) {
            (void)n_qubits;
            PyErr_WarnEx(
                PyExc_DeprecationWarning,
                "The argument n_qubits is no longer needed. "
                "Please create ToffoliBoxes without n_qubits.",
                1);
            new (p) ToffoliBox(
                to_cpp_state_perm_t(perm), ToffoliBoxSynthStrat::Matching,
                rotation_axis);
          },
          "Constructor for backward compatibility. Subject to deprecation.",
          nb::arg("n_qubits"), nb::arg("permutation"),
          nb::arg("rotation_axis") = OpType::Ry)
      .def(
          "__init__",
          [](ToffoliBox *p, const py_state_perm_t2 &perm,
             ToffoliBoxSynthStrat strat, OpType optype) {
            new (p) ToffoliBox(to_cpp_state_perm_t(perm), strat, optype);
          },
          "Construct from a permutation of basis states\n\n"
          ":param permutation: a map between bitstrings\n"
          ":param strat: synthesis strategy\n"
          ":param rotation_axis: the rotation axis of the multiplexors used in "
          "the decomposition. Can be either Rx or Ry. Only applicable to the "
          "Matching strategy. Default to Ry.",
          nb::arg("permutation"), nb::arg("strat"),
          nb::arg("rotation_axis") = OpType::Ry)
      .def(
          "__init__",
          [](ToffoliBox *p, const py_state_perm_t2 &perm,
             const OpType &rotation_axis) {
            new (p) ToffoliBox(
                to_cpp_state_perm_t(perm), ToffoliBoxSynthStrat::Matching,
                rotation_axis);
          },
          "Construct from a permutation of basis states and perform synthesis "
          "using the Matching strategy\n\n"
          ":param permutation: a map between bitstrings\n"
          ":param rotation_axis: the rotation axis of the multiplexors used in "
          "the decomposition. Can be either Rx or Ry, default to Ry.",
          nb::arg("permutation"), nb::arg("rotation_axis") = OpType::Ry)
      .def(
          "__init__",
          [](ToffoliBox *p, unsigned n_qubits, const py_state_perm_t2 &perm,
             const OpType &rotation_axis) {
            (void)n_qubits;
            PyErr_WarnEx(
                PyExc_DeprecationWarning,
                "The argument n_qubits is no longer needed. "
                "Please create ToffoliBoxes without n_qubits.",
                1);
            new (p) ToffoliBox(
                to_cpp_state_perm_t(perm), ToffoliBoxSynthStrat::Matching,
                rotation_axis);
          },
          "Constructor for backward compatibility. Subject to deprecation.",
          nb::arg("n_qubits"), nb::arg("permutation"),
          nb::arg("rotation_axis") = OpType::Ry)
      .def(
          "get_circuit", [](ToffoliBox &tbox) { return *tbox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_permutation",
          [](ToffoliBox &box) {
            std::map<nb::tuple, nb::tuple> outmap;
            for (const auto &pair : box.get_permutation()) {
              outmap.insert(
                  {nb::tuple(nb::cast(pair.first)),
                   nb::tuple(nb::cast(pair.second))});
            }
            return outmap;
          },
          ":return: the permutation")
      .def(
          "get_strat", &ToffoliBox::get_strat,
          ":return: the synthesis strategy")
      .def(
          "get_rotation_axis", &ToffoliBox::get_rotation_axis,
          ":return: the rotation axis");
  nb::class_<ResourceBounds<unsigned>>(
      m, "ResourceBounds",
      "Structure holding a minimum and maximum value of some resource, where "
      "both values are unsigned integers.")
      .def(
          "__init__",
          [](ResourceBounds<unsigned> *p, unsigned min, unsigned max) {
            if (min > max) {
              throw std::invalid_argument(
                  "minimum must be less than or equal to maximum");
            }
            new (p) ResourceBounds<unsigned>{min, max};
          },
          "Constructs a ResourceBounds object.\n\n"
          ":param min: minimum value\n"
          ":param max: maximum value\n",
          nb::arg("min"), nb::arg("max"))
      .def(
          "get_min",
          [](const ResourceBounds<unsigned> &resource_bounds) {
            return resource_bounds.min;
          },
          ":return: the minimum value")
      .def(
          "get_max",
          [](const ResourceBounds<unsigned> &resource_bounds) {
            return resource_bounds.max;
          },
          ":return: the maximum value");
  nb::class_<ResourceData>(
      m, "ResourceData",
      "An object holding resource data for use in a :py:class:`~.DummyBox`."
      "\n\nThe object holds several fields representing minimum and maximum "
      "values for certain resources. The absence of an :py:class:`~.OpType` in "
      "one of these fields is interpreted as the absence of gates of that type "
      "in the (imagined) circuit."
      "\n\nSee :py:meth:`~.Circuit.get_resources` for how to use this data.")
      .def(
          nb::init<
              const std::map<OpType, ResourceBounds<unsigned>> &,
              const ResourceBounds<unsigned> &,
              const std::map<OpType, ResourceBounds<unsigned>> &,
              const ResourceBounds<unsigned> &>(),
          "Constructs a ResourceData object.\n\n"
          ":param op_type_count: dictionary of counts of selected "
          ":py:class:`~.OpType`\n"
          ":param gate_depth: overall gate depth\n"
          ":param op_type_depth: dictionary of depths of selected "
          ":py:class:`~.OpType`\n"
          ":param two_qubit_gate_depth: overall two-qubit-gate depth",
          nb::arg("op_type_count"), nb::arg("gate_depth"),
          nb::arg("op_type_depth"), nb::arg("two_qubit_gate_depth"))
      .def(
          "get_op_type_count",
          [](const ResourceData &resource_data) {
            return resource_data.OpTypeCount;
          },
          ":return: bounds on the op type count")
      .def(
          "get_gate_depth",
          [](const ResourceData &resource_data) {
            return resource_data.GateDepth;
          },
          ":return: bounds on the gate depth")
      .def(
          "get_op_type_depth",
          [](const ResourceData &resource_data) {
            return resource_data.OpTypeDepth;
          },
          ":return: bounds on the op type depth")
      .def(
          "get_two_qubit_gate_depth",
          [](const ResourceData &resource_data) {
            return resource_data.TwoQubitGateDepth;
          },
          ":return: bounds on the two-qubit-gate depth")
      .def("__repr__", [](const ResourceData &resource_data) {
        std::stringstream ss;
        ss << "ResourceData(";
        ss << "op_type_count={";
        for (const auto &pair : resource_data.OpTypeCount) {
          ss << "OpType." << optypeinfo().at(pair.first).name << ": "
             << "ResourceBounds(" << pair.second.min << ", " << pair.second.max
             << "), ";
        }
        ss << "}, ";
        ss << "gate_depth=ResourceBounds(" << resource_data.GateDepth.min
           << ", " << resource_data.GateDepth.max << "), ";
        ss << "op_type_depth={";
        for (const auto &pair : resource_data.OpTypeDepth) {
          ss << "OpType." << optypeinfo().at(pair.first).name << ": "
             << "ResourceBounds(" << pair.second.min << ", " << pair.second.max
             << "), ";
        }
        ss << "}, ";
        ss << "two_qubit_gate_depth=ResourceBounds("
           << resource_data.TwoQubitGateDepth.min << ", "
           << resource_data.TwoQubitGateDepth.max << ")";
        ss << ")";
        return ss.str();
      });
  nb::class_<DummyBox, Op>(
      m, "DummyBox",
      "A placeholder operation that holds resource data. This box type cannot "
      "be decomposed into a circuit. It only serves to record resource data "
      "for a region of a circuit: for example, upper and lower bounds on gate "
      "counts and depth. A circuit containing such a box cannot be executed.")
      .def(
          nb::init<unsigned, unsigned, const ResourceData &>(),
          "Construct a new instance from some resource data.",
          nb::arg("n_qubits"), nb::arg("n_bits"), nb::arg("resource_data"))
      .def(
          "get_n_qubits", &DummyBox::get_n_qubits,
          ":return: the number of qubits covered by the box")
      .def(
          "get_n_bits", &DummyBox::get_n_bits,
          ":return: the number of bits covered by the box")
      .def(
          "get_resource_data", &DummyBox::get_resource_data,
          ":return: the associated resource data");
  nb::class_<QControlBox, Op>(
      m, "QControlBox",
      "A user-defined controlled operation specified by an "
      ":py:class:`~.Op`, the number of quantum controls, and the control state "
      "expressed as an integer or a bit vector.")
      .def(
          nb::init<Op_ptr &, unsigned, std::vector<bool> &>(),
          "Construct from an :py:class:`~.Op`, a number of quantum "
          "controls, and the control state expressed as a bit vector. The "
          "controls occupy the low-index ports of the "
          "resulting operation.\n\n"
          ":param op: the underlying operator\n"
          ":param n_controls: the number of control qubits. Default to 1\n"
          ":param control_state: the control state expressed as a bit vector. "
          "Default to all 1s\n",
          nb::arg("op"), nb::arg("n_controls") = 1,
          nb::arg("control_state") = std::vector<bool>())
      .def(
          "__init__",
          [](QControlBox *p, Op_ptr &op, unsigned n_controls,
             unsigned long long control_state) {
            new (p) QControlBox(
                op, n_controls, dec_to_bin(control_state, n_controls));
          },
          "Construct from an :py:class:`~.Op`, a number of quantum "
          "controls, and the control state expressed as an integer. The "
          "controls occupy the low-index ports of the "
          "resulting operation.\n\n"
          ":param op: the underlying operator\n"
          ":param n_controls: the number of control qubits\n"
          ":param control_state: the control state expressed as an integer. "
          "Big-endian\n",
          nb::arg("op"), nb::arg("n_controls"), nb::arg("control_state"))
      .def(
          nb::init<Op_ptr &, unsigned>(),
          "Construct from an :py:class:`~.Op` and a number of quantum "
          "controls. The controls occupy the low-index ports of the "
          "resulting operation.",
          nb::arg("op"), nb::arg("n") = 1)
      .def(
          "get_circuit", [](QControlBox &qcbox) { return *qcbox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def("get_op", &QControlBox::get_op, ":return: the underlying operator")
      .def(
          "get_n_controls", &QControlBox::get_n_controls,
          ":return: the number of control qubits")
      .def(
          "get_control_state",
          [](QControlBox &qcbox) {
            return bin_to_dec(qcbox.get_control_state());
          },
          ":return: the control state as an integer (big-endian binary "
          "representation)")
      .def(
          "get_control_state_bits",
          [](QControlBox &qcbox) { return qcbox.get_control_state(); },
          ":return: the control state as a bit vector");

  nb::class_<CompositeGateDef>(
      m, "CustomGateDef",
      "A custom unitary gate definition, given as a composition of other "
      "gates")
      .def(
          nb::init<
              const std::string &, const Circuit &,
              const nb::tket_custom::SequenceVec<Sym> &>())
      .def_static(
          "define",
          [](const std::string &name, const Circuit &def,
             const nb::tket_custom::SequenceVec<Sym> &args) {
            return CompositeGateDef::define_gate(name, def, args);
          },
          "Define a new custom gate as a composite of other "
          "gates\n\n:param name: Readable name for the new "
          "gate\n:param circ: The definition of the gate as a "
          "Circuit\n:param args: Symbols to be encapsulated as "
          "arguments of the custom gate",
          nb::arg("name"), nb::arg("circ"), nb::arg("args"))
      .def_prop_ro(
          "name", &CompositeGateDef::get_name, "The readable name of the gate")
      .def_prop_ro(
          "definition", &CompositeGateDef::get_def,
          "Return definition as a circuit.")
      .def_prop_ro(
          "args", &CompositeGateDef::get_args,
          "Return symbolic arguments of gate.")
      .def_prop_ro(
          "arity", &CompositeGateDef::n_args,
          "The number of real parameters for the gate")
      .def(
          "to_dict",
          [](const CompositeGateDef &c) {
            return nb::cast<nb::dict>(
                nb::object(json(std::make_shared<CompositeGateDef>(c))));
          },
          ":return: a JSON serializable dictionary representation of "
          "the CustomGateDef")
      .def_static(
          "from_dict",
          [](const nb::dict &composite_gate_def_dict) {
            return json(composite_gate_def_dict).get<composite_def_ptr_t>();
          },
          "Construct Circuit instance from JSON serializable "
          "dictionary representation of the Circuit.");
  nb::class_<CustomGate, Op>(
      m, "CustomGate",
      "A user-defined gate defined by a parametrised :py:class:`~.Circuit`.")
      .def(
          nb::init<
              const composite_def_ptr_t &,
              const nb::tket_custom::SequenceVec<Expr> &>(),
          "Instantiate a custom gate.", nb::arg("gatedef"), nb::arg("params"))
      .def_prop_ro(
          "name", [](CustomGate &cgate) { return cgate.get_name(false); },
          "The readable name of the gate.")
      .def_prop_ro(
          "params", &CustomGate::get_params, "The parameters of the gate.")
      .def_prop_ro("gate", &CustomGate::get_gate, "Underlying gate object.")
      .def(
          "get_circuit",
          [](CustomGate &composite) { return *composite.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the gate.");
  nb::class_<PhasePolyBox, Op>(
      m, "PhasePolyBox",
      "Box encapsulating any Circuit made up of CNOT and RZ as a phase "
      "polynomial + linear transformation")
      .def(
          "__init__",
          [](PhasePolyBox *p, unsigned n_qb,
             const std::map<Qubit, unsigned> &q_ind,
             const PyPhasePolynomial &p_p, const MatrixXb &lin_trans) {
            boost::bimap<Qubit, unsigned> bmap;
            for (const auto &pair : q_ind) {
              bmap.insert({pair.first, pair.second});
            }
            new (p) PhasePolyBox(n_qb, bmap, to_cpp_phase_poly(p_p), lin_trans);
          },
          "\n\nConstruct from the number of qubits, the mapping from "
          "Qubit to index, the phase polynomial (map from bitstring "
          "to phase) and the linear transformation (boolean matrix)",
          nb::arg("n_qubits"), nb::arg("qubit_indices"),
          nb::arg("phase_polynomial"), nb::arg("linear_transformation"))
      .def(
          "__init__",
          [](PhasePolyBox *p, unsigned n_qb,
             const std::map<Qubit, unsigned> &q_ind,
             const PyPhasePolynomialAlternate &p_p, const MatrixXb &lin_trans) {
            boost::bimap<Qubit, unsigned> bmap;
            for (const auto &pair : q_ind) {
              bmap.insert({pair.first, pair.second});
            }
            new (p) PhasePolyBox(n_qb, bmap, to_cpp_phase_poly(p_p), lin_trans);
          },
          "Construct from the number of qubits, the mapping from "
          "Qubit to index, the phase polynomial (list of bitstring "
          "phase pairs) and the linear transformation (boolean matrix)\n\n"
          "If any bitstring is repeated in the phase polynomial list, the last "
          "given value for that bistring will be used",
          nb::arg("n_qubits"), nb::arg("qubit_indices"),
          nb::arg("phase_polynomial"), nb::arg("linear_transformation"))
      .def(
          nb::init<const Circuit &>(),
          "Construct a PhasePolyBox from a given circuit containing only Rz "
          "and CX gates.",
          nb::arg("circuit"))
      .def_prop_ro(
          "n_qubits", &PhasePolyBox::get_n_qubits,
          "Number of gates the polynomial acts on.")
      .def_prop_ro(
          "phase_polynomial_as_list",
          [](PhasePolyBox &ppoly) {
            const PhasePolynomial &phase_pol = ppoly.get_phase_polynomial();
            return cast_map_to_vector_of_pairs(phase_pol);
          },
          "List of bitstring(basis state)-phase pairs.")
      .def_prop_ro(
          "phase_polynomial",
          [](PhasePolyBox &ppoly) {
            const PhasePolynomial &phase_pol = ppoly.get_phase_polynomial();
            return cast_keys_to_tuples(phase_pol);
          },
          "Map from bitstring (basis state) to phase.")
      .def_prop_ro(
          "phase_polynomial_as_list",
          [](PhasePolyBox &ppoly) {
            const PhasePolynomial &phase_pol = ppoly.get_phase_polynomial();
            return cast_map_to_vector_of_pairs(phase_pol);
          },
          "List of bitstring(basis state)-phase pairs.")
      .def_prop_ro(
          "linear_transformation", &PhasePolyBox::get_linear_transformation,
          "Boolean matrix corresponding to linear transformation.")
      .def(
          "get_circuit", [](PhasePolyBox &ppb) { return *ppb.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box.")
      .def_prop_ro(
          "qubit_indices",
          [](PhasePolyBox &ppoly) {
            const boost::bimap<Qubit, unsigned> &bmap =
                ppoly.get_qubit_indices();
            std::map<Qubit, unsigned> outmap;
            for (const auto &pair : bmap.left) {
              outmap.insert({pair.first, pair.second});
            }
            return outmap;
          },
          "Map from Qubit to index in polynomial.");
  nb::class_<ProjectorAssertionBox, Op>(
      m, "ProjectorAssertionBox",
      "A user-defined assertion specified by a 2x2, 4x4, or 8x8 projector "
      "matrix.")
      .def(
          nb::init<const Eigen::MatrixXcd &, BasisOrder>(),
          "Construct from a projector matrix.\n\n"
          ":param m: The projector matrix\n"
          ":param basis: Whether the provided unitary is in the ILO-BE "
          "(increasing lexicographic order of qubit ids, big-endian "
          "indexing) format, or DLO-BE (decreasing lexicographic order "
          "of ids)",
          nb::arg("m"), nb::arg("basis") = BasisOrder::ilo)
      .def(
          "get_circuit",
          [](ProjectorAssertionBox &ubox) { return *ubox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_matrix", &ProjectorAssertionBox::get_matrix,
          ":return: the unitary matrix (in ILO-BE format) as a numpy array");
  nb::class_<StabiliserAssertionBox, Op>(
      m, "StabiliserAssertionBox",
      "A user-defined assertion specified by a list of Pauli stabilisers.")
      .def(
          nb::init<const nb::tket_custom::SequenceVec<PauliStabiliser>>(),
          "Construct from a list of Pauli stabilisers.\n\n"
          ":param stabilisers: The list of Pauli stabilisers\n",
          nb::arg("stabilisers"))
      .def(
          "__init__",
          [](StabiliserAssertionBox *p,
             const nb::tket_custom::SequenceVec<std::string> &pauli_strings) {
            PauliStabiliserVec stabilisers;
            for (auto &raw_string : pauli_strings) {
              std::vector<Pauli> string;
              quarter_turns_t coeff = 0;
              for (unsigned i = 0; i < raw_string.size(); i++) {
                switch (raw_string[i]) {
                  case '-':
                    if (i == 0) {
                      coeff = 2;
                    } else {
                      throw std::invalid_argument(
                          "Invalid Pauli string: " + raw_string);
                    }
                    break;
                  case 'I':
                    string.push_back(Pauli::I);
                    break;
                  case 'X':
                    string.push_back(Pauli::X);
                    break;
                  case 'Y':
                    string.push_back(Pauli::Y);
                    break;
                  case 'Z':
                    string.push_back(Pauli::Z);
                    break;
                  default:
                    throw std::invalid_argument(
                        "Invalid Pauli string: " + raw_string);
                }
              }
              stabilisers.emplace_back(string, coeff);
            }
            new (p) StabiliserAssertionBox(stabilisers);
          },
          "Construct from a list of Pauli stabilisers.\n\n"
          ":param m: The list of Pauli stabilisers expressed as Python "
          "strings\n",
          nb::arg("stabilisers"))
      .def(
          "get_circuit",
          [](StabiliserAssertionBox &ubox) { return *ubox.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_stabilisers", &StabiliserAssertionBox::get_stabilisers,
          ":return: the list of Pauli stabilisers");

  nb::class_<MultiplexorBox, Op>(
      m, "MultiplexorBox",
      "A user-defined multiplexor (i.e. uniformly controlled operations) "
      "specified by a "
      "map from bitstrings to " CLSOBJS(~.Op)
      "or a list of bitstring-" CLSOBJS(~.Op) " pairs")
      .def(
        "__init__",
        [](MultiplexorBox *p, const py_ctrl_op_map_t_alt & bitstring_op_pairs) {
          new(p) MultiplexorBox(to_cpp_ctrl_op_map_t(bitstring_op_pairs));
        },
      "Construct from a list of bitstring-" CLSOBJS(~.Op) "pairs\n\n"
      ":param bitstring_to_op_list: List of bitstring-" CLSOBJS(~.Op) "pairs\n",
      nb::arg("bistring_to_op_list"))
      .def(
          "__init__",
          [](MultiplexorBox *p, const py_ctrl_op_map_t & bitstring_op_map) {
            new(p) MultiplexorBox(to_cpp_ctrl_op_map_t(bitstring_op_map));
          },
          "Construct from a map from bitstrings to " CLSOBJS(~.Op) "\n\n"
          ":param op_map: Map from bitstrings to " CLSOBJS(~.Op) "\n",
          nb::arg("op_map"))
      .def(
          "get_circuit", [](MultiplexorBox &box) { return *box.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_op_map",
          [](MultiplexorBox &box) {
            return cast_keys_to_tuples(box.get_op_map());
          },
          ":return: the underlying op map")
      .def(
          "get_bitstring_op_pair_list",
          [](MultiplexorBox &box) {
          return cast_map_to_vector_of_pairs(box.get_op_map());
          },
          ":return: the underlying bistring-op pairs");
  nb::class_<
      MultiplexedRotationBox, Op>(
      m, "MultiplexedRotationBox",
      "A user-defined multiplexed rotation gate (i.e. "
      "uniformly controlled single-axis rotations) specified by "
      "a map from bitstrings to " CLSOBJS(~.Op)
      "or a list of bitstring-" CLSOBJS(~.Op) " pairs. "
      "Implementation based on arxiv.org/abs/quant-ph/0410066. "
      "The decomposed circuit has at most 2^k single-qubit rotations, "
      "2^k CX gates, and two additional H gates if the rotation axis is X. "
      "k is the number of control qubits.")
      .def(
          "__init__",
          [](MultiplexedRotationBox *p, const py_ctrl_op_map_t_alt & bitstring_op_pairs) {
            new(p) MultiplexedRotationBox(to_cpp_ctrl_op_map_t(bitstring_op_pairs));
          },
          "Construct from a list of bitstring-" CLSOBJS(~.Op) "pairs\n\n"
          "All " CLSOBJS(~.Op) "  must share the same single-qubit rotation type: "
          "Rx, Ry, or Rz.\n\n"
          ":param bitstring_to_op_list: List of bitstring-" CLSOBJS(~.Op) "pairs\n",
          nb::arg("bistring_to_op_list"))
      .def(
          "__init__",
          [](MultiplexedRotationBox *p, const py_ctrl_op_map_t & bitstring_op_map) {
            new(p) MultiplexedRotationBox(to_cpp_ctrl_op_map_t(bitstring_op_map));
          },
          "Construct from a map from bitstrings to :py:class:`~.Op` s."
          "All " CLSOBJS(~.Op) "  must share the same single-qubit rotation type: "
          "Rx, Ry, or Rz.\n\n"
          ":param op_map: Map from bitstrings to " CLSOBJS(~.Op) "\n",
          nb::arg("op_map"))
      .def(
          "__init__",
          [](MultiplexedRotationBox *p, const nb::tket_custom::SequenceVec<double> &angles, const OpType &axis) {
            if (angles.size() == 0) {
              throw std::invalid_argument("Angles are empty.");
            }
            if (angles.size() & (angles.size() - 1)) {
              throw std::invalid_argument(
                  "The size of the angles is not power of 2.");
            }
            if (axis != OpType::Rx && axis != OpType::Ry &&
                axis != OpType::Rz) {
              throw std::invalid_argument(
                  "The axis must be either Rx, Ry, or Rz.");
            }
            unsigned bitstring_width = (unsigned)log2(angles.size());
            ctrl_op_map_t op_map;
            for (unsigned i = 0; i < angles.size(); i++) {
              if (std::abs(angles[i]) > EPS) {
                std::vector<bool> bits = dec_to_bin(i, bitstring_width);
                op_map.insert({bits, get_op_ptr(axis, angles[i])});
              }
            }
            new(p) MultiplexedRotationBox(op_map);
          },
          "Construct from a list of angles and the rotation axis.\n\n"
          ":param angles: List of rotation angles in half-turns. angles[i] is "
          "the angle activated by the binary representation of i\n"
          ":param axis: ``OpType.Rx``, ``OpType.Ry`` or ``OpType.Rz``\n",
          nb::arg("angles"), nb::arg("axis"))
      .def(
          "get_circuit",
          [](MultiplexedRotationBox &box) { return *box.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_bitstring_op_pair_list",
          [](MultiplexedRotationBox &box) {
            return cast_map_to_vector_of_pairs(box.get_op_map());
          },
          ":return: the underlying bistring-op pairs")
      .def(
          "get_op_map",
          [](MultiplexedRotationBox &box) {
            return cast_keys_to_tuples(box.get_op_map());
          },
          ":return: the underlying op map");
  nb::class_<MultiplexedU2Box, Op>(
      m, "MultiplexedU2Box",
      "A user-defined multiplexed U2 gate (i.e. uniformly controlled U2 "
      "gate) specified by a "
      "map from bitstrings to " CLSOBJS(~.Op)
      "or a list of bitstring-" CLSOBJS(~.Op) " pairs"
      "Implementation based on arxiv.org/abs/quant-ph/0410066. "
      "The decomposed circuit has at most 2^k single-qubit gates, 2^k -1 CX gates, "
      "and a k+1 qubit DiagonalBox at the end. "
      "k is the number of control qubits.")
      .def(
          "__init__",
          [](MultiplexedU2Box *p, const py_ctrl_op_map_t_alt & bitstring_op_pairs, bool impl_diag) {
            new(p) MultiplexedU2Box(to_cpp_ctrl_op_map_t(bitstring_op_pairs), impl_diag);
          },
      "Construct from a list of bitstring-" CLSOBJS(~.Op) "pairs\n\n"
      "Only supports single qubit unitary gate types and "
      ":py:class:`~.Unitary1qBox`.\n\n"
      ":param op_map: List of bitstring-" CLSOBJS(~.Op) "pairs\n"
      ":param impl_diag: Whether to implement the final diagonal gate, "
      "default to True.",
      nb::arg("bistring_to_op_list"), nb::arg("impl_diag") = true)
      .def(
          "__init__",
          [](MultiplexedU2Box *p, const py_ctrl_op_map_t & bitstring_op_map, bool impl_diag) {
            new(p) MultiplexedU2Box(to_cpp_ctrl_op_map_t(bitstring_op_map), impl_diag);
          },
          "Construct from a map from bitstrings to " CLSOBJS(~.Op) "."
          "Only supports single qubit unitary gate types and "
          ":py:class:`~.Unitary1qBox`.\n\n"
          ":param op_map: Map from bitstrings to " CLSOBJS(~.Op) "\n"
          ":param impl_diag: Whether to implement the final diagonal gate, "
          "default to True.",
          nb::arg("op_map"), nb::arg("impl_diag") = true)
      .def(
          "get_circuit",
          [](MultiplexedU2Box &box) { return *box.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_bitstring_op_pair_list",
          [](MultiplexedU2Box &box) {
          return cast_map_to_vector_of_pairs(box.get_op_map());
          },
          ":return: the underlying bistring-op pairs")
      .def(
          "get_op_map",
          [](MultiplexedU2Box &box) {
            return cast_keys_to_tuples(box.get_op_map());
          },
          ":return: the underlying op map")
      .def(
          "get_impl_diag", &MultiplexedU2Box::get_impl_diag,
          ":return: flag indicating whether to implement the final diagonal "
          "gate.");

  nb::class_<MultiplexedTensoredU2Box, Op>(
      m, "MultiplexedTensoredU2Box",
      "A user-defined multiplexed tensor product of U2 gates specified by a "
      "map from bitstrings to lists of " CLSOBJS(~.Op)
      "or a list of bitstring-list(" CLSOBJS(~.Op) ") pairs. "
      "A box with k control qubits and t target qubits is implemented as t "
      "k-controlled multiplexed-U2 gates with their diagonal "
      "components merged and commuted to the end. The resulting circuit contains "
      "t non-diagonal components of the multiplexed-U2 decomposition, t k-controlled "
      "multiplexed-Rz boxes, and a k-qubit DiagonalBox at the end. "
      "The total CX count is at most 2^k(2t+1)-t-2."
      )
      .def(
          "__init__",
          [](MultiplexedTensoredU2Box *p, const py_ctrl_tensored_op_map_t_alt & bitstring_op_pairs) {
            new(p) MultiplexedTensoredU2Box(to_cpp_ctrl_op_map_t(bitstring_op_pairs));
          },
          "Construct from a list of bitstring-" CLSOBJS(~.Op) "pairs\n\n"
          "Only supports single qubit unitary gate types and "
          ":py:class:`~.Unitary1qBox`.\n\n"
          ":param bitstring_to_op_list: List of bitstring-List of " CLSOBJS(~.Op) " pairs\n",
          nb::arg("bistring_to_op_list"))
      .def(
          "__init__",
          [](MultiplexedTensoredU2Box *p, const py_ctrl_tensored_op_map_t & bitstring_op_map) {
            new(p) MultiplexedTensoredU2Box(to_cpp_ctrl_op_map_t(bitstring_op_map));
          },
          "Construct from a map from bitstrings to equal-sized lists of "
          CLSOBJS(~.Op) ". "
          "Only supports single qubit unitary gate types and "
          ":py:class:`~.Unitary1qBox`.\n\n"
          ":param op_map: Map from bitstrings to lists of " CLSOBJS(~.Op),
          nb::arg("op_map"))
      .def(
          "get_circuit",
          [](MultiplexedTensoredU2Box &box) { return *box.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_bitstring_op_pair_list",
          [](MultiplexedTensoredU2Box &box) {
          return cast_map_to_vector_of_pairs(box.get_op_map());
          },
          ":return: the underlying bistring-op pairs")
      .def(
          "get_op_map",
          [](MultiplexedTensoredU2Box &box) {
            return cast_keys_to_tuples(box.get_op_map());
          },
          ":return: the underlying op map");
  nb::class_<StatePreparationBox, Op>(
      m, "StatePreparationBox",
      "A box for preparing quantum states using multiplexed-Ry and "
      "multiplexed-Rz gates. "
      "Implementation based on Theorem 9 of "
      "arxiv.org/abs/quant-ph/0406176. "
      "The decomposed circuit has at most 2*(2^n-2) CX gates, and "
      "2^n-2 CX gates if the coefficients are all real.")
      .def(
          nb::init<const Eigen::VectorXcd &, bool, bool>(),
          "Construct from a statevector\n\n"
          ":param statevector: normalised statevector\n"
          ":param is_inverse: whether to implement the dagger of the state "
          "preparation circuit, default to false\n"
          ":param with_initial_reset: whether to explicitly set the state to "
          "zero initially (by default the initial zero state is assumed and no "
          "explicit reset is applied)",
          nb::arg("statevector"), nb::arg("is_inverse") = false,
          nb::arg("with_initial_reset") = false)
      .def(
          "get_circuit",
          [](StatePreparationBox &box) { return *box.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_statevector", &StatePreparationBox::get_statevector,
          ":return: the statevector")
      .def(
          "is_inverse", &StatePreparationBox::is_inverse,
          ":return: flag indicating whether to implement the dagger of the "
          "state preparation circuit")
      .def(
          "with_initial_reset", &StatePreparationBox::with_initial_reset,
          ":return: flag indicating whether the qubits are explicitly "
          "set to the zero state initially");
  nb::class_<DiagonalBox, Op>(
      m, "DiagonalBox",
      "A box for synthesising a diagonal unitary matrix into a sequence of "
      "multiplexed-Rz gates. "
      "Implementation based on Theorem 7 of "
      "arxiv.org/abs/quant-ph/0406176. "
      "The decomposed circuit has at most 2^n-2 CX gates.")
      .def(
          nb::init<const Eigen::VectorXcd &, bool>(),
          "Construct from the diagonal entries of the unitary operator. The "
          "size of the vector must be 2^n where n is a positive integer.\n\n"
          ":param diagonal: diagonal entries\n"
          ":param upper_triangle: indicates whether the multiplexed-Rz gates "
          "take the shape of an upper triangle or a lower triangle. Default to "
          "true.",
          nb::arg("diagonal"), nb::arg("upper_triangle") = true)
      .def(
          "get_circuit", [](DiagonalBox &box) { return *box.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_diagonal", &DiagonalBox::get_diagonal,
          ":return: the statevector")
      .def(
          "is_upper_triangle", &DiagonalBox::is_upper_triangle,
          ":return: the upper_triangle flag");
  nb::class_<ConjugationBox, Op>(
      m, "ConjugationBox",
      "A box to express computations that follow the compute-action-uncompute "
      "pattern.")
      .def(
          nb::init<
              const Op_ptr &, const Op_ptr &, const std::optional<Op_ptr>>(),
          "Construct from operations that perform compute, action, and "
          "uncompute. All three operations need to be quantum and have the "
          "same size.\n\n"
          ":param compute: the compute operation\n"
          ":param action: the action operation\n"
          ":param uncompute: optional uncompute operation, default to "
          "compute.dagger(). If provided, the user needs to make sure that "
          "uncompute.dagger() and compute have the same unitary.",
          nb::arg("compute"), nb::arg("action"),
          nb::arg("uncompute") = std::nullopt)
      .def(
          "get_circuit", [](ConjugationBox &box) { return *box.to_circuit(); },
          ":return: the :py:class:`~.Circuit` described by the box")
      .def(
          "get_compute", &ConjugationBox::get_compute,
          ":return: the compute operation")
      .def(
          "get_action", &ConjugationBox::get_action,
          ":return: the action operation")
      .def(
          "get_uncompute", &ConjugationBox::get_uncompute,
          ":return: the uncompute operation. Returns None if the default "
          "compute.dagger() is used");
}
}  // namespace tket

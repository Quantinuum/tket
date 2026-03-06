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
#include <nanobind/nanobind.h>

#include <tket/Circuit/Circuit.hpp>

#include "binder_utils.hpp"
#include "deleted_hash.hpp"
#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"
#include "py_operators.hpp"
#include "tket/Circuit/Command.hpp"
#include "tket/Gate/OpPtrFunctions.hpp"
#include "tket/Gate/SymTable.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Ops/BarrierOp.hpp"
#include "tket/Ops/MetaOp.hpp"
#include "tket/Ops/Op.hpp"
#include "tket/Utils/Constants.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

typedef nb::tket_custom::SequenceVec<EdgeType> py_op_signature_t;
typedef nb::tket_custom::SequenceVec<UnitID> py_unit_vector_t;

void def_circuit(nb::class_<Circuit> &);
void init_classical(nb::module_ &m);
void init_boxes(nb::module_ &m);
void init_clexpr(nb::module_ &m);

NB_MODULE(circuit, m) {
  nb::set_leak_warnings(false);
  nb::module_::import_("pytket._tket.unit_id");
  nb::module_::import_("pytket._tket.pauli");
  nb::module_::import_("pytket._tket.architecture");
  nb::enum_<CXConfigType>(
      m, "CXConfigType",
      "Enum for available configurations for CXs upon decompose phase "
      "gadgets")
      .value(
          "Snake", CXConfigType::Snake,
          "linear nearest neighbour CX sequence. Linear depth.")
      .value(
          "Star", CXConfigType::Star,
          "Every CX has same target, linear depth, good for gate "
          "cancellation.")
      .value(
          "Tree", CXConfigType::Tree,
          "Balanced tree: logarithmic depth, harder to route.")
      .value(
          "MultiQGate", CXConfigType::MultiQGate,
          "Support for multi-qubit architectures, decomposing to 3-qubit "
          "XXPhase3 gates instead of CXs where possible.");
  nb::enum_<EdgeType>(
      m, "EdgeType", "Type of a wire in a circuit or input to an op")
      .value("Boolean", EdgeType::Boolean)
      .value("Classical", EdgeType::Classical)
      .value("Quantum", EdgeType::Quantum)
      .value("WASM", EdgeType::WASM)
      .value("RNG", EdgeType::RNG);
  // NOTE: Sphinx does not automatically pick up the docstring for OpType
  nb::enum_<OpType>(
      m, "OpType",
      "Enum for available operations compatible with tket " CLSOBJS(
          ~.Circuit) ".",
      nb::is_arithmetic())
      .value(
          "Phase", OpType::Phase,
          "Global phase: :math:`(\\alpha) \\mapsto \\left[ \\begin{array}{c} "
          "e^{i\\pi\\alpha} \\end{array} \\right]`")
      .value(
          "Z", OpType::Z,
          "Pauli Z: :math:`\\left[ \\begin{array}{cc} 1 & 0 \\\\ 0 & "
          "-1 \\end{array} \\right]`")
      .value(
          "X", OpType::X,
          "Pauli X: :math:`\\left[ \\begin{array}{cc} 0 & 1 \\\\ 1 & "
          "0 \\end{array} \\right]`")
      .value(
          "Y", OpType::Y,
          "Pauli Y: :math:`\\left[ \\begin{array}{cc} 0 & -i \\\\ i & "
          "0 \\end{array} \\right]`")
      .value(
          "S", OpType::S,
          ":math:`\\left[ \\begin{array}{cc} 1 & 0 \\\\ 0 & i "
          "\\end{array} \\right] = \\mathrm{U1}(\\frac12)`")
      .value(
          "Sdg", OpType::Sdg,
          ":math:`\\mathrm{S}^{\\dagger} = \\left[ \\begin{array}{cc} "
          "1 & 0 \\\\ 0 & -i \\end{array} \\right] = "
          "\\mathrm{U1}(-\\frac12)`")
      .value(
          "T", OpType::T,
          ":math:`\\left[ \\begin{array}{cc} 1 & 0 \\\\ 0 & "
          "e^{i\\pi/4} \\end{array} \\right] = "
          "\\mathrm{U1}(\\frac14)`")
      .value(
          "Tdg", OpType::Tdg,
          ":math:`\\mathrm{T}^{\\dagger} = \\left[ \\begin{array}{cc} "
          "1 & 0 \\\\ 0 & e^{-i\\pi/4} \\end{array} \\right] = "
          "\\mathrm{U1}(-\\frac14)`")
      .value(
          "V", OpType::V,
          ":math:`\\frac{1}{\\sqrt 2} \\left[ \\begin{array}{cc} 1 & "
          "-i \\\\ -i & 1 \\end{array} \\right] = "
          "\\mathrm{Rx}(\\frac12)`")
      .value(
          "Vdg", OpType::Vdg,
          ":math:`\\mathrm{V}^{\\dagger} = \\frac{1}{\\sqrt 2} "
          "\\left[ \\begin{array}{cc} 1 & i \\\\ i & 1 \\end{array} "
          "\\right] = \\mathrm{Rx}(-\\frac12)`")
      .value(
          "SX", OpType::SX,
          ":math:`\\frac{1}{2} \\left[ \\begin{array}{cc} 1 + i & "
          "1 - i \\\\ 1 - i & 1 + i \\end{array} \\right] = "
          "e^{\\frac{i\\pi}{4}}\\mathrm{Rx}(\\frac12)`")
      .value(
          "SXdg", OpType::SXdg,
          ":math:`\\mathrm{SX}^{\\dagger} = \\frac{1}{2} "
          "\\left[ \\begin{array}{cc} 1 - i & 1 + i "
          "\\\\ 1 + i & 1 - i \\end{array} "
          "\\right] = e^{\\frac{-i\\pi}{4}}\\mathrm{Rx}(-\\frac12)`")
      .value(
          "H", OpType::H,
          "Hadamard gate: :math:`\\frac{1}{\\sqrt 2} \\left[ "
          "\\begin{array}{cc} 1 & 1 \\\\ 1 & -1 \\end{array} "
          "\\right]`")
      .value(
          "Rx", OpType::Rx,
          ":math:`(\\alpha) \\mapsto e^{-\\frac12 i \\pi \\alpha "
          "\\mathrm{X}} = \\left[ \\begin{array}{cc} "
          "\\cos\\frac{\\pi\\alpha}{2} & "
          "-i\\sin\\frac{\\pi\\alpha}{2} \\\\ "
          "-i\\sin\\frac{\\pi\\alpha}{2} & "
          "\\cos\\frac{\\pi\\alpha}{2} \\end{array} \\right]`")
      .value(
          "Ry", OpType::Ry,
          ":math:`(\\alpha) \\mapsto e^{-\\frac12 i \\pi \\alpha "
          "\\mathrm{Y}} = \\left[ \\begin{array}{cc} "
          "\\cos\\frac{\\pi\\alpha}{2} & -\\sin\\frac{\\pi\\alpha}{2} "
          "\\\\ \\sin\\frac{\\pi\\alpha}{2} & "
          "\\cos\\frac{\\pi\\alpha}{2} \\end{array} \\right]`")
      .value(
          "Rz", OpType::Rz,
          ":math:`(\\alpha) \\mapsto e^{-\\frac12 i \\pi \\alpha "
          "\\mathrm{Z}} = \\left[ \\begin{array}{cc} e^{-\\frac12 i "
          "\\pi\\alpha} & 0 \\\\ 0 & e^{\\frac12 i \\pi\\alpha} "
          "\\end{array} \\right]`")
      .value(
          "U1", OpType::U1,
          ":math:`(\\lambda) \\mapsto \\mathrm{U3}(0, 0, \\lambda) = "
          "e^{\\frac12 i\\pi\\lambda} \\mathrm{Rz}(\\lambda)`. "
          "U-gates are used by IBM. See "
          "https://qiskit.org/documentation/tutorials/circuits/"
          "3_summary_of_quantum_operations.html "
          "for more information on U-gates.")
      .value(
          "U2", OpType::U2,
          ":math:`(\\phi, \\lambda) \\mapsto \\mathrm{U3}(\\frac12, "
          "\\phi, \\lambda) = e^{\\frac12 i\\pi(\\lambda+\\phi)} "
          "\\mathrm{Rz}(\\phi) \\mathrm{Ry}(\\frac12) "
          "\\mathrm{Rz}(\\lambda)`, defined by matrix multiplication")
      .value(
          "U3", OpType::U3,
          ":math:`(\\theta, \\phi, \\lambda) \\mapsto  \\left[ "
          "\\begin{array}{cc} \\cos\\frac{\\pi\\theta}{2} & "
          "-e^{i\\pi\\lambda} \\sin\\frac{\\pi\\theta}{2} \\\\ "
          "e^{i\\pi\\phi} \\sin\\frac{\\pi\\theta}{2} & "
          "e^{i\\pi(\\lambda+\\phi)} \\cos\\frac{\\pi\\theta}{2} "
          "\\end{array} \\right] = e^{\\frac12 i\\pi(\\lambda+\\phi)} "
          "\\mathrm{Rz}(\\phi) \\mathrm{Ry}(\\theta) "
          "\\mathrm{Rz}(\\lambda)`")
      .value(
          "GPI", OpType::GPI,
          ":math:`(\\phi) \\mapsto \\left[ \\begin{array}{cc} 0 & "
          "e^{-i\\pi\\phi} \\\\ e^{i\\pi\\phi} & 0 \\end{array} \\right]`")
      .value(
          "GPI2", OpType::GPI2,
          ":math:`(\\phi) \\mapsto \\frac{1}{\\sqrt 2} \\left[ "
          "\\begin{array}{cc} 1 & -ie^{-i\\pi\\phi} \\\\ -ie^{i\\pi\\phi} & "
          "1 \\end{array} \\right]`")
      .value(
          "AAMS", OpType::AAMS,
          ":math:`(\\theta, \\phi_0, \\phi_1) \\mapsto \\left[ "
          "\\begin{array}{cccc} \\cos\\frac{\\pi\\theta}{2} & 0 & 0 & "
          "-ie^{-i\\pi(\\phi_0+\\phi_1)}\\sin\\frac{\\pi\\theta}{2} \\\\ "
          "0 & "
          "\\cos\\frac{\\pi\\theta}{2} & "
          "-ie^{i\\pi(\\phi_1-\\phi_0)}\\sin\\frac{\\pi\\theta}{2} & 0 \\\\ 0 "
          "& "
          "-ie^{i\\pi(\\phi_0-\\phi_1)}\\sin\\frac{\\pi\\theta}{2} & "
          "\\cos\\frac{\\pi\\theta}{2} & 0 \\\\ "
          "-ie^{i\\pi(\\phi_0+\\phi_1)}\\sin\\frac{\\pi\\theta}{2} & 0 & 0 & "
          "\\cos\\frac{\\pi\\theta}{2} \\end{array} \\right]`")
      .value(
          "TK1", OpType::TK1,
          ":math:`(\\alpha, \\beta, \\gamma) \\mapsto "
          "\\mathrm{Rz}(\\alpha) \\mathrm{Rx}(\\beta) "
          "\\mathrm{Rz}(\\gamma)`")
      .value(
          "TK2", OpType::TK2,
          ":math:`(\\alpha, \\beta, \\gamma) \\mapsto "
          "\\mathrm{XXPhase}(\\alpha) "
          "\\mathrm{YYPhase}(\\beta) "
          "\\mathrm{ZZPhase}(\\gamma)`")
      .value("CX", OpType::CX, "Controlled :math:`\\mathrm{X}` gate")
      .value("CY", OpType::CY, "Controlled :math:`\\mathrm{Y}` gate")
      .value("CZ", OpType::CZ, "Controlled :math:`\\mathrm{Z}` gate")
      .value("CH", OpType::CH, "Controlled :math:`\\mathrm{H}` gate")
      .value("CV", OpType::CV, "Controlled :math:`\\mathrm{V}` gate")
      .value(
          "CVdg", OpType::CVdg,
          "Controlled :math:`\\mathrm{V}^{\\dagger}` gate")
      .value("CSX", OpType::CSX, "Controlled :math:`\\mathrm{SX}` gate")
      .value(
          "CSXdg", OpType::CSXdg,
          "Controlled :math:`\\mathrm{SX}^{\\dagger}` gate")
      .value("CS", OpType::CS, "Controlled :math:`\\mathrm{S}` gate")
      .value(
          "CSdg", OpType::CSdg,
          "Controlled :math:`\\mathrm{S}^{\\dagger}` gate")
      .value(
          "CRz", OpType::CRz,
          ":math:`(\\alpha) \\mapsto` Controlled "
          ":math:`\\mathrm{Rz}(\\alpha)` gate")
      .value(
          "CRx", OpType::CRx,
          ":math:`(\\alpha) \\mapsto` Controlled "
          ":math:`\\mathrm{Rx}(\\alpha)` gate")
      .value(
          "CRy", OpType::CRy,
          ":math:`(\\alpha) \\mapsto` Controlled "
          ":math:`\\mathrm{Ry}(\\alpha)` gate")
      .value(
          "CU1", OpType::CU1,
          ":math:`(\\lambda) \\mapsto` Controlled "
          ":math:`\\mathrm{U1}(\\lambda)` gate. Note that this is not "
          "equivalent to a :math:`\\mathrm{CRz}(\\lambda)` up to "
          "global phase, differing by an extra "
          ":math:`\\mathrm{Rz}(\\frac{\\lambda}{2})` on the control "
          "qubit.")
      .value(
          "CU3", OpType::CU3,
          ":math:`(\\theta, \\phi, \\lambda) \\mapsto` Controlled "
          ":math:`\\mathrm{U3}(\\theta, \\phi, \\lambda)` gate. "
          "Similar rules apply.")
      .value(
          "PhaseGadget", OpType::PhaseGadget,
          ":math:`\\alpha \\mapsto e^{-\\frac12 i \\pi\\alpha Z^{\\otimes n}}`")
      .value("CCX", OpType::CCX, "Toffoli gate")
      .value(
          "ECR", OpType::ECR,
          ":math:`\\frac{1}{\\sqrt 2} \\left[ "
          "\\begin{array}{cccc} 0 & 0 & 1 & i \\\\"
          "0 & 0 & i & 1 \\\\"
          "1 & -i & 0 & 0 \\\\"
          "-i & 1 & 0 & 0 \\end{array} \\right]`")
      .value("SWAP", OpType::SWAP, "Swap gate")
      .value("CSWAP", OpType::CSWAP, "Controlled swap gate")
      .value(
          "noop", OpType::noop,
          "Identity gate. These gates are not permanent and are "
          "automatically stripped by the compiler")
      .value(
          "Barrier", OpType::Barrier,
          "Meta-operation preventing compilation through it. Not "
          "automatically stripped by the compiler")
      .value(
          "Label", OpType::Label,
          "Label for control flow jumps. Does not appear within a "
          "circuit")
      .value(
          "Branch", OpType::Branch,
          "A control flow jump to a label dependent on the value of a "
          "given Bit. Does not appear within a circuit")
      .value(
          "Goto", OpType::Goto,
          "An unconditional control flow jump to a Label. Does not "
          "appear within a circuit.")
      .value(
          "Stop", OpType::Stop,
          "Halts execution immediately. Used to terminate a program. "
          "Does not appear within a circuit.")
      .value(
          "BRIDGE", OpType::BRIDGE,
          "A CX Bridge over 3 qubits. Used to apply a logical CX "
          "between the first and third qubits when they are not "
          "adjacent on the device, but both neighbour the second "
          "qubit. Acts as the identity on the second qubit")
      .value(
          "Measure", OpType::Measure,
          "Z-basis projective measurement, storing the measurement "
          "outcome in a specified bit")
      .value(
          "Reset", OpType::Reset,
          "Resets the qubit to :math:`\\left|0\\right>`")
      .value("CircBox", OpType::CircBox, "Represents an arbitrary subcircuit")
      .value(
          "PhasePolyBox", OpType::PhasePolyBox,
          "An operation representing arbitrary circuits made up of CX and Rz "
          "gates, represented as a phase polynomial together with a boolean "
          "matrix representing an additional linear transformation.")
      .value(
          "Unitary1qBox", OpType::Unitary1qBox,
          "Represents an arbitrary one-qubit unitary operation by its "
          "matrix")
      .value(
          "Unitary2qBox", OpType::Unitary2qBox,
          "Represents an arbitrary two-qubit unitary operation by its "
          "matrix")
      .value(
          "Unitary3qBox", OpType::Unitary3qBox,
          "Represents an arbitrary three-qubit unitary operation by its matrix")
      .value(
          "ExpBox", OpType::ExpBox,
          "A two-qubit operation corresponding to a unitary matrix "
          "defined as the exponential :math:`e^{itA}` of an arbitrary "
          "4x4 hermitian matrix :math:`A`.")
      .value(
          "PauliExpBox", OpType::PauliExpBox,
          "An operation defined as the exponential "
          ":math:`e^{-\\frac{i\\pi\\alpha}{2} P}` of a tensor "
          ":math:`P` of Pauli operations.")
      .value(
          "PauliExpPairBox", OpType::PauliExpPairBox,
          "A pair of (not necessarily commuting) Pauli exponentials "
          ":math:`e^{-\\frac{i\\pi\\alpha}{2} P}` performed in sequence.")
      .value(
          "PauliExpCommutingSetBox", OpType::PauliExpCommutingSetBox,
          "An operation defined as a set"
          "of commuting exponentials of the form "
          ":math:`e^{-\\frac{i\\pi\\alpha}{2} P}` of a tensor "
          ":math:`P` of Pauli operations.")
      .value(
          "TermSequenceBox", OpType::TermSequenceBox,
          "An unordered collection of Pauli exponentials "
          "that can be synthesised in any order, causing a "
          "change in the unitary operation. Synthesis order "
          "depends on the synthesis strategy chosen only.")
      .value(
          "QControlBox", OpType::QControlBox,
          "An arbitrary n-controlled operation")
      .value(
          "ToffoliBox", OpType::ToffoliBox,
          "A permutation of classical basis states")
      .value(
          "ConjugationBox", OpType::ConjugationBox,
          "An operation composed of 'action', 'compute' and 'uncompute' "
          "circuits")
      .value(
          "DummyBox", OpType::DummyBox,
          "A placeholder operation that holds resource data")
      .value(
          "CustomGate", OpType::CustomGate,
          ":math:`(\\alpha, \\beta, \\ldots) \\mapsto` A user-defined "
          "operation, based on a :py:class:`~.Circuit` :math:`C` with "
          "parameters :math:`\\alpha, \\beta, \\ldots` substituted in "
          "place of bound symbolic variables in :math:`C`, as defined "
          "by the :py:class:`~.CustomGateDef`.")
      .value(
          "Conditional", OpType::Conditional,
          "An operation to be applied conditionally on the value of "
          "some classical register")
      .value(
          "ISWAP", OpType::ISWAP,
          ":math:`(\\alpha) \\mapsto e^{\\frac14 i \\pi\\alpha "
          "(\\mathrm{X} \\otimes \\mathrm{X} + \\mathrm{Y} \\otimes "
          "\\mathrm{Y})} = \\left[ \\begin{array}{cccc} 1 & 0 & 0 & 0 "
          "\\\\ 0 & \\cos\\frac{\\pi\\alpha}{2} & "
          "i\\sin\\frac{\\pi\\alpha}{2} & 0 \\\\ 0 & "
          "i\\sin\\frac{\\pi\\alpha}{2} & \\cos\\frac{\\pi\\alpha}{2} "
          "& 0 \\\\ 0 & 0 & 0 & 1 \\end{array} \\right]`")
      .value(
          "PhasedISWAP", OpType::PhasedISWAP,
          ":math:`(p, t) \\mapsto \\left[ \\begin{array}{cccc} 1 & 0 "
          "& 0 & 0 \\\\ 0 & \\cos\\frac{\\pi t}{2} & "
          "i\\sin\\frac{\\pi t}{2}e^{2i\\pi p} & 0 \\\\ 0 & "
          "i\\sin\\frac{\\pi t}{2}e^{-2i\\pi p} & \\cos\\frac{\\pi "
          "t}{2} & 0 \\\\ 0 & 0 & 0 & 1 \\end{array} \\right]` "
          "(equivalent to: Rz(p)[0]; Rz(-p)[1]; ISWAP(t); "
          "Rz(-p)[0]; Rz(p)[1])")
      .value(
          "TwinPhasedX", OpType::TwinPhasedX,
          ":math:`(\\alpha, \\beta) \\mapsto \\mathrm{PhasedX}(\\alpha, "
          "\\beta) \\otimes \\mathrm{PhasedX}(\\alpha, \\beta)`")
      .value(
          "PhasedXX", OpType::PhasedXX,
          ":math:`(\\alpha, beta) \\mapsto \\mathrm{AAMS}(\\alpha, -\\beta, "
          "-\\beta) = \\left[ \\begin{array}{cccc} "
          "\\cos\\frac{\\pi\\alpha}{2} & 0 & 0 & "
          "-i\\sin\\frac{\\pi\\alpha}{2} e^{2i\\pi\\beta} \\\\ 0 & "
          "\\cos\\frac{\\pi\\alpha}{2} & "
          "-i\\sin\\frac{\\pi\\alpha}{2} & 0 \\\\ 0 & "
          "-i\\sin\\frac{\\pi\\alpha}{2} & "
          "\\cos\\frac{\\pi\\alpha}{2} & 0 \\\\ "
          "-i\\sin\\frac{\\pi\\alpha}{2} e^{-2i\\pi\\beta} & 0 & 0 & "
          "\\cos\\frac{\\pi\\alpha}{2} \\end{array} \\right]`")
      .value(
          "XXPhase", OpType::XXPhase,
          ":math:`(\\alpha) \\mapsto e^{-\\frac12 i \\pi\\alpha "
          "(\\mathrm{X} \\otimes \\mathrm{X})} = \\left[ "
          "\\begin{array}{cccc} \\cos\\frac{\\pi\\alpha}{2} & 0 & 0 & "
          "-i\\sin\\frac{\\pi\\alpha}{2} \\\\ 0 & "
          "\\cos\\frac{\\pi\\alpha}{2} & "
          "-i\\sin\\frac{\\pi\\alpha}{2} & 0 \\\\ 0 & "
          "-i\\sin\\frac{\\pi\\alpha}{2} & "
          "\\cos\\frac{\\pi\\alpha}{2} & 0 \\\\ "
          "-i\\sin\\frac{\\pi\\alpha}{2} & 0 & 0 & "
          "\\cos\\frac{\\pi\\alpha}{2} \\end{array} \\right]`")
      .value(
          "YYPhase", OpType::YYPhase,
          ":math:`(\\alpha) \\mapsto e^{-\\frac12 i \\pi\\alpha "
          "(\\mathrm{Y} \\otimes \\mathrm{Y})} = \\left[ "
          "\\begin{array}{cccc} \\cos\\frac{\\pi\\alpha}{2} & 0 & 0 & "
          "i\\sin\\frac{\\pi\\alpha}{2} \\\\ 0 & "
          "\\cos\\frac{\\pi\\alpha}{2} & "
          "-i\\sin\\frac{\\pi\\alpha}{2} & 0 \\\\ 0 & "
          "-i\\sin\\frac{\\pi\\alpha}{2} & "
          "\\cos\\frac{\\pi\\alpha}{2} & 0 \\\\ "
          "i\\sin\\frac{\\pi\\alpha}{2} & 0 & 0 & "
          "\\cos\\frac{\\pi\\alpha}{2} \\end{array} \\right]`")
      .value(
          "ZZPhase", OpType::ZZPhase,
          ":math:`(\\alpha) \\mapsto e^{-\\frac12 i \\pi\\alpha "
          "(\\mathrm{Z} \\otimes \\mathrm{Z})} = \\left[ "
          "\\begin{array}{cccc} e^{-\\frac12 i \\pi\\alpha} & 0 & 0 & "
          "0 \\\\ 0 & e^{\\frac12 i \\pi\\alpha} & 0 & 0 \\\\ 0 & 0 & "
          "e^{\\frac12 i \\pi\\alpha} & 0 \\\\ 0 & 0 & 0 & "
          "e^{-\\frac12 i \\pi\\alpha} \\end{array} \\right]`")
      .value(
          "XXPhase3", OpType::XXPhase3,
          "A 3-qubit gate XXPhase3(α) consists of pairwise 2-qubit XXPhase(α) "
          "interactions. "
          "Equivalent to XXPhase(α)[0, 1] XXPhase(α)[1, 2] XXPhase(α)[0, 2].")
      .value(
          "PhasedX", OpType::PhasedX,
          ":math:`(\\alpha,\\beta) \\mapsto "
          "\\mathrm{Rz}(\\beta)\\mathrm{Rx}(\\alpha)\\mathrm{Rz}(-"
          "\\beta)` (matrix-multiplication order)")
      .value(
          "NPhasedX", OpType::NPhasedX,
          ":math:`(\\alpha, \\beta) \\mapsto \\mathrm{PhasedX}(\\alpha, \\beta)"
          "^{\\otimes n}` (n-qubit gate composed of identical PhasedX in "
          "parallel.")
      .value(
          "CnRx", OpType::CnRx,
          ":math:`(\\alpha)` := n-controlled "
          ":math:`\\mathrm{Rx}(\\alpha)` gate.")
      .value(
          "CnRy", OpType::CnRy,
          ":math:`(\\alpha)` := n-controlled "
          ":math:`\\mathrm{Ry}(\\alpha)` gate.")
      .value(
          "CnRz", OpType::CnRz,
          ":math:`(\\alpha)` := n-controlled "
          ":math:`\\mathrm{Rz}(\\alpha)` gate.")
      .value("CnX", OpType::CnX, "n-controlled X gate.")
      .value("CnY", OpType::CnY, "n-controlled Y gate.")
      .value("CnZ", OpType::CnZ, "n-controlled Z gate.")
      .value(
          "ZZMax", OpType::ZZMax,
          ":math:`e^{-\\frac{i\\pi}{4}(\\mathrm{Z} \\otimes "
          "\\mathrm{Z})}`, a maximally entangling ZZPhase")
      .value(
          "ESWAP", OpType::ESWAP,
          ":math:`\\alpha \\mapsto e^{-\\frac12 i\\pi\\alpha \\cdot "
          "\\mathrm{SWAP}} = \\left[ \\begin{array}{cccc} "
          "e^{-\\frac12 i \\pi\\alpha} & 0 & 0 & 0 \\\\ 0 & "
          "\\cos\\frac{\\pi\\alpha}{2} & "
          "-i\\sin\\frac{\\pi\\alpha}{2} & 0 \\\\ 0 & "
          "-i\\sin\\frac{\\pi\\alpha}{2} & "
          "\\cos\\frac{\\pi\\alpha}{2} & 0 \\\\ 0 & 0 & 0 & "
          "e^{-\\frac12 i \\pi\\alpha} \\end{array} \\right]`")
      .value(
          "FSim", OpType::FSim,
          ":math:`(\\alpha, \\beta) \\mapsto \\left[ "
          "\\begin{array}{cccc} 1 & 0 & 0 & 0 \\\\ 0 & "
          "\\cos \\pi\\alpha & "
          "-i\\sin \\pi\\alpha & 0 \\\\ 0 & "
          "-i\\sin \\pi\\alpha & "
          "\\cos \\pi\\alpha & 0 \\\\ 0 & 0 & 0 & "
          "e^{-i\\pi\\beta} \\end{array} \\right]`")
      .value(
          "Sycamore", OpType::Sycamore,
          ":math:`\\mathrm{FSim}(\\frac12, \\frac16)`")
      .value(
          "ISWAPMax", OpType::ISWAPMax,
          ":math:`\\mathrm{ISWAP}(1) = \\left[ \\begin{array}{cccc} 1 "
          "& 0 & 0 & 0 \\\\ 0 & 0 & i & 0 \\\\ 0 & i & 0 & 0 \\\\ 0 & "
          "0 & 0 & 1 \\end{array} \\right]`")
      .value(
          "ClassicalTransform", OpType::ClassicalTransform,
          "A general classical operation where all inputs are also outputs")
      .value(
          "WASM", OpType::WASM, "Op containing a classical wasm function call")
      .value(
          "SetBits", OpType::SetBits,
          "An operation to set some bits to specified values")
      .value(
          "CopyBits", OpType::CopyBits, "An operation to copy some bit values")
      .value(
          "RangePredicate", OpType::RangePredicate,
          "A classical predicate defined by a range of values in binary "
          "encoding")
      .value(
          "ExplicitPredicate", OpType::ExplicitPredicate,
          "A classical predicate defined by a truth table")
      .value(
          "ExplicitModifier", OpType::ExplicitModifier,
          "An operation defined by a truth table that modifies one bit")
      .value(
          "MultiBit", OpType::MultiBit,
          "A classical operation applied to multiple bits simultaneously")
      .value(
          "MultiplexorBox", OpType::MultiplexorBox,
          "A multiplexor (i.e. uniformly controlled operations)")
      .value(
          "MultiplexedRotationBox", OpType::MultiplexedRotationBox,
          "A multiplexed rotation gate (i.e. "
          "uniformly controlled single-axis rotations)")
      .value(
          "MultiplexedU2Box", OpType::MultiplexedU2Box,
          "A multiplexed U2 gate (i.e. uniformly controlled U2 gate)")
      .value(
          "MultiplexedTensoredU2Box", OpType::MultiplexedTensoredU2Box,
          "A multiplexed tensored-U2 gate")
      .value(
          "StatePreparationBox", OpType::StatePreparationBox,
          "A box for preparing quantum states using multiplexed-Ry and "
          "multiplexed-Rz gates")
      .value(
          "DiagonalBox", OpType::DiagonalBox,
          "A box for synthesising a diagonal unitary matrix into a sequence of "
          "multiplexed-Rz gates")
      .value("ClExpr", OpType::ClExpr, "A classical expression")
      .value("Input", OpType::Input, "Quantum input node of the circuit")
      .value("Output", OpType::Output, "Quantum output node of the circuit")
      .value(
          "Create", OpType::Create,
          "Quantum node with no predecessors, "
          "implicitly in zero state")
      .value(
          "Discard", OpType::Discard,
          "Quantum node with no successors, not "
          "composable with input nodes of other circuits")
      .value("ClInput", OpType::ClInput, "Classical input node of the circuit")
      .value(
          "ClOutput", OpType::ClOutput,
          "Classical output node of the "
          "circuit")
      .value("WASMInput", OpType::WASMInput, "WASM input node of the circuit")
      .value(
          "WASMOutput", OpType::WASMOutput,
          "WASM output node of the "
          "circuit")
      .value(
          "Collapse", OpType::Collapse,
          "Measure a qubit producing no "
          "output")
      .value("CliffBox", OpType::CliffBox, "NYI")
      .value(
          "ProjectorAssertionBox", OpType::ProjectorAssertionBox,
          "See "
          ":py:class:`~.ProjectorAssertionBox`")
      .value(
          "StabiliserAssertionBox", OpType::StabiliserAssertionBox,
          "See "
          ":py:class:`~.StabiliserAssertionBox`")
      .value(
          "UnitaryTableauBox", OpType::UnitaryTableauBox,
          "See "
          ":py:class:`~.UnitaryTableauBox`")
      .value("RNGInput", OpType::RNGInput, "RNG input node")
      .value("RNGOutput", OpType::RNGOutput, "RNG output node")
      .value("RNGSeed", OpType::RNGSeed, "Seed an RNG using 64 bits")
      .value(
          "RNGBound", OpType::RNGBound,
          "Set an (inclusive) 32-bit upper bound on RNG output")
      .value("RNGIndex", OpType::RNGIndex, "Set a 32-bit index on an RNG")
      .value("RNGNum", OpType::RNGNum, "Get 32-bit output from an RNG")
      .value(
          "JobShotNum", OpType::JobShotNum,
          "Get 32-bit (little-endian) shot number when a circuit is being run "
          "multiple times")
      .def_static(
          "from_name",
          [](const nb::str &name) { return json(name).get<OpType>(); },
          "Construct from name");
  nb::class_<Op>(m, "Op", "Encapsulates operation information")
      .def_static(
          "create",
          [](OpType optype) { return get_op_ptr(optype, std::vector<Expr>()); },
          "Create an :py:class:`~.Op` with given type")
      .def_static(
          "create",
          [](OpType optype, const Expr &param) {
            return get_op_ptr(optype, param);
          },
          "Create an :py:class:`~.Op` with given type and parameter")
      .def_static(
          "create",
          [](OpType optype, const nb::tket_custom::SequenceVec<Expr> &params) {
            return get_op_ptr(optype, params);
          },
          "Create an :py:class:`~.Op` with given type and parameters")
      .def_prop_ro("type", &Op::get_type, "Type of op being performed")
      .def_prop_ro(
          "params", &Op::get_params_reduced,
          "Angular parameters of the op, in half-turns (e.g. 1.0 "
          "half-turns is :math:`\\pi` radians). The parameters "
          "returned are constrained to the appropriate canonical "
          "range, which is usually the half-open interval [0,2) but "
          "for some operations (e.g. Rx, Ry and Rz) is [0,4).")
      .def_prop_ro("n_qubits", &Op::n_qubits, "Number of qubits of op")
      .def_prop_ro("dagger", &Op::dagger, "Dagger of op")
      .def_prop_ro("transpose", &Op::transpose, "Transpose of op")
      .def(
          "get_name", &Op::get_name, "String representation of op",
          nb::arg("latex") = false)
      .def("__eq__", &py_equals<Op>)
      .def("__hash__", &deletedHash<Op>, deletedHashDocstring)
      .def("__repr__", [](const Op &op) { return op.get_name(); })
      .def("free_symbols", [](const Op &op) { return op.free_symbols(); })
      .def("get_unitary", [](const Op *op) { return op->get_unitary(); })
      .def(
          "is_clifford_type",
          [](const Op &op) { return op.get_desc().is_clifford_gate(); },
          "Check if the operation is one of the Clifford " CLSOBJS(
              ~.OpType) ".")
      .def(
          "is_clifford", [](const Op &op) { return op.is_clifford(); },
          "Test whether the operation is in the Clifford group. A return value "
          "of true guarantees that the operation is Clifford. However, the "
          "converse is not the case as some Clifford operations may not be "
          "detected as such.")
      .def("is_gate", [](const Op &op) { return op.get_desc().is_gate(); });

  nb::enum_<BasisOrder>(
      m, "BasisOrder",
      "Enum for readout basis and ordering.\n"
      "Readouts are viewed in increasing lexicographic order (ILO) of "
      "the bit's UnitID. This is our default convention for column "
      "indexing for ALL readout forms (shots, counts, statevector, and "
      "unitaries). e.g. :math:`\\lvert abc \\rangle` corresponds to the "
      "readout: ('c', 0) --> :math:`a`, ('c', 1) --> :math:`b`, ('d', 0) "
      "--> :math:`c`\n"
      "For statevector and unitaries, the string abc is interpreted as "
      "an index in a big-endian (BE) fashion. e.g. the statevector "
      ":math:`(a_{00}, a_{01}, a_{10}, a_{11})`\n"
      "Some backends (Qiskit, ProjectQ, etc.) use a DLO-BE (decreasing "
      "lexicographic order, big-endian) convention. This is the same as "
      "ILO-LE (little-endian) for statevectors and unitaries, but gives "
      "shot tables/readouts in a counter-intuitive manner.\n"
      "Every backend and matrix-based box has a BasisOrder option which "
      "can toggle between ILO-BE (ilo) and DLO-BE (dlo).")
      .value(
          "ilo", BasisOrder::ilo,
          "Increasing Lexicographic Order of UnitID, big-endian")
      .value(
          "dlo", BasisOrder::dlo,
          "Decreasing Lexicographic Order of UnitID, big-endian");

  nb::class_<Command>(
      m, "Command",
      "A single quantum command in the circuit, defined by the Op, the "
      "qubits it acts on, and the op group name if any.")
      .def(
          nb::init<const Op_ptr, py_unit_vector_t>(),
          "Construct from an operation and a vector of unit IDs", nb::arg("op"),
          nb::arg("args"))
      .def("__eq__", &py_equals<Command>)
      .def("__hash__", &deletedHash<Command>, deletedHashDocstring)
      .def("__repr__", &Command::to_str)
      .def_prop_ro("op", &Command::get_op_ptr, "Operation for this command.")
      .def_prop_ro(
          "args", &Command::get_args, "The qubits/bits the command acts on.")
      .def_prop_ro(
          "qubits", &Command::get_qubits, "The qubits the command acts on.")
      .def_prop_ro(
          "bits", &Command::get_bits,
          "The bits the command could write to (does not include "
          "read-only bits).")
      .def_prop_ro(
          "opgroup", &Command::get_opgroup,
          "The op group name assigned to the command (or `None` if "
          "no name is defined).")
      .def(
          "free_symbols",
          [](const Command &com) { return com.get_op_ptr()->free_symbols(); },
          ":return: set of symbolic parameters for the command");

  nb::class_<MetaOp, Op>(
      m, "MetaOp", "Meta operation, such as input or output vertices.")
      .def(
          nb::init<OpType, py_op_signature_t, const std::string &>(),
          "Construct MetaOp with optype, signature and additional data string"
          "\n\n:param type: type for the meta op"
          "\n:param signature: signature for the op"
          "\n:param data: additional string stored in the op",
          nb::arg("type"), nb::arg("signature"), nb::arg("data"))
      .def_prop_ro("data", &MetaOp::get_data, "Get data from MetaOp");

  nb::class_<BarrierOp, Op>(m, "BarrierOp", "Barrier operations.")
      .def(
          nb::init<py_op_signature_t, const std::string &>(),
          "Construct BarrierOp with signature and additional data string"
          "\n:param signature: signature for the op"
          "\n:param data: additional string stored in the op",
          nb::arg("signature"), nb::arg("data"))
      .def_prop_ro("data", &BarrierOp::get_data, "Get data from BarrierOp");

  auto pyCircuit = nb::class_<Circuit>(
      m, "Circuit", nb::dynamic_attr(),
      "Encapsulates a quantum circuit using a DAG representation.\n\n>>> "
      "from pytket import Circuit\n>>> c = Circuit(4,2) # Create a circuit "
      "with 4 qubits and 2 classical bits"
      "\n>>> c.H(0) # Apply a gate to qubit 0\n>>> "
      "c.Rx(0.5,1) # Angles of rotation are expressed in half-turns "
      "(i.e. 0.5 means PI/2)\n>>> c.Measure(1,0) # Measure qubit 1, saving "
      "result in bit 0");
  init_boxes(m);
  init_classical(m);
  init_clexpr(m);
  def_circuit(pyCircuit);

  m.def(
      "fresh_symbol", &SymTable::fresh_symbol,
      "Given some preferred symbol, this finds an appropriate suffix "
      "that "
      "will guarantee it has not yet been used in the current python "
      "session.\n\n:param preferred: The preferred readable symbol name "
      "as "
      "a string (default is 'a')\n\n:return: A new sympy symbol object",
      nb::arg("preferred") = 'a');
}

}  // namespace tket

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

#include "tket/OpType/OpTypeFunctions.hpp"

#include <memory>

#include "tket/OpType/OpType.hpp"

namespace tket {

bool find_in_set(const OpType& val, const OpTypeSet& set) {
  return set.find(val) != set.cend();
}

const OpTypeSet& all_gate_types() {
  static const OpTypeSet optypes{
      OpType::Z,           OpType::X,           OpType::Y,
      OpType::S,           OpType::Sdg,         OpType::T,
      OpType::Tdg,         OpType::V,           OpType::Vdg,
      OpType::SX,          OpType::SXdg,        OpType::H,
      OpType::Rx,          OpType::Ry,          OpType::Rz,
      OpType::U3,          OpType::U2,          OpType::U1,
      OpType::TK1,         OpType::CX,          OpType::CY,
      OpType::CZ,          OpType::CH,          OpType::CV,
      OpType::CVdg,        OpType::CSX,         OpType::CSXdg,
      OpType::CS,          OpType::CSdg,        OpType::CRz,
      OpType::CRx,         OpType::CRy,         OpType::CU1,
      OpType::CU3,         OpType::PhaseGadget, OpType::CCX,
      OpType::SWAP,        OpType::CSWAP,       OpType::noop,
      OpType::Measure,     OpType::Reset,       OpType::ECR,
      OpType::ISWAP,       OpType::PhasedX,     OpType::ZZMax,
      OpType::XXPhase,     OpType::YYPhase,     OpType::ZZPhase,
      OpType::CnRx,        OpType::CnRz,        OpType::CnRy,
      OpType::CnX,         OpType::CnZ,         OpType::CnY,
      OpType::BRIDGE,      OpType::Collapse,    OpType::ESWAP,
      OpType::FSim,        OpType::Sycamore,    OpType::ISWAPMax,
      OpType::PhasedISWAP, OpType::XXPhase3,    OpType::NPhasedX,
      OpType::TK2,         OpType::Phase,       OpType::GPI,
      OpType::GPI2,        OpType::AAMS,        OpType::TwinPhasedX,
      OpType::PhasedXX};
  static std::unique_ptr<const OpTypeSet> gates =
      std::make_unique<const OpTypeSet>(optypes);
  return *gates;
}

const OpTypeSet& all_multi_qubit_types() {
  static const OpTypeSet optypes{
      OpType::CX,          OpType::CY,       OpType::CZ,
      OpType::CH,          OpType::CV,       OpType::CVdg,
      OpType::CSX,         OpType::CSXdg,    OpType::CS,
      OpType::CSdg,        OpType::CRz,      OpType::CRx,
      OpType::CRy,         OpType::CU1,      OpType::CU3,
      OpType::PhaseGadget, OpType::CCX,      OpType::SWAP,
      OpType::CSWAP,       OpType::ECR,      OpType::ISWAP,
      OpType::ZZMax,       OpType::XXPhase,  OpType::YYPhase,
      OpType::ZZPhase,     OpType::CnRx,     OpType::CnRz,
      OpType::CnRy,        OpType::CnX,      OpType::CnZ,
      OpType::CnY,         OpType::BRIDGE,   OpType::ESWAP,
      OpType::FSim,        OpType::Sycamore, OpType::ISWAPMax,
      OpType::PhasedISWAP, OpType::XXPhase3, OpType::NPhasedX,
      OpType::TK2,         OpType::AAMS,     OpType::TwinPhasedX,
      OpType::XXPhase};
  static std::unique_ptr<const OpTypeSet> gates =
      std::make_unique<const OpTypeSet>(optypes);
  return *gates;
}

// the set of OpTypes that implement Gate_ptr->get_tk1_angles()
const OpTypeSet& all_single_qubit_unitary_types() {
  static const OpTypeSet optypes{
      OpType::noop, OpType::Z,    OpType::X,   OpType::Y,  OpType::S,
      OpType::Sdg,  OpType::T,    OpType::Tdg, OpType::V,  OpType::Vdg,
      OpType::SX,   OpType::SXdg, OpType::H,   OpType::Rx, OpType::Ry,
      OpType::Rz,   OpType::U1,   OpType::U2,  OpType::U3, OpType::PhasedX,
      OpType::TK1,  OpType::GPI,  OpType::GPI2};
  static std::unique_ptr<const OpTypeSet> gates =
      std::make_unique<const OpTypeSet>(optypes);
  return *gates;
}

const OpTypeSet& all_single_qubit_types() {
  static const OpTypeSet optypes{
      OpType::Z,     OpType::X,        OpType::Y,       OpType::S,
      OpType::Sdg,   OpType::T,        OpType::Tdg,     OpType::V,
      OpType::Vdg,   OpType::SX,       OpType::SXdg,    OpType::H,
      OpType::Rx,    OpType::Ry,       OpType::Rz,      OpType::U3,
      OpType::U2,    OpType::U1,       OpType::TK1,     OpType::Measure,
      OpType::Reset, OpType::Collapse, OpType::PhasedX, OpType::noop,
      OpType::GPI,   OpType::GPI2};
  static std::unique_ptr<const OpTypeSet> gates =
      std::make_unique<const OpTypeSet>(optypes);
  return *gates;
}

const OpTypeSet& all_classical_types() {
  static const OpTypeSet optypes = {
      OpType::ClassicalTransform,
      OpType::SetBits,
      OpType::CopyBits,
      OpType::RangePredicate,
      OpType::ExplicitPredicate,
      OpType::ExplicitModifier,
      OpType::MultiBit,
      OpType::WASM,
      OpType::ClExpr,
      OpType::RNGSeed,
      OpType::RNGBound,
      OpType::RNGIndex,
      OpType::RNGNum,
      OpType::JobShotNum,
  };
  static std::unique_ptr<const OpTypeSet> gates =
      std::make_unique<const OpTypeSet>(optypes);
  return *gates;
}

const OpTypeSet& all_projective_types() {
  static const OpTypeSet optypes{
      OpType::Measure, OpType::Collapse, OpType::Reset};
  static std::unique_ptr<const OpTypeSet> gates =
      std::make_unique<const OpTypeSet>(optypes);
  return *gates;
}

const OpTypeSet& all_controlled_gate_types() {
  static const OpTypeSet optypes{
      OpType::CX,   OpType::CCX,   OpType::CnX,  OpType::CnZ,  OpType::CnY,
      OpType::CSX,  OpType::CSXdg, OpType::CS,   OpType::CSdg, OpType::CV,
      OpType::CVdg, OpType::CRx,   OpType::CnRx, OpType::CnRz, OpType::CnRy,
      OpType::CRy,  OpType::CY,    OpType::CRz,  OpType::CZ,   OpType::CH,
      OpType::CU1,  OpType::CU3};
  static std::unique_ptr<const OpTypeSet> gates =
      std::make_unique<const OpTypeSet>(optypes);
  return *gates;
}

const OpTypeSet& all_opaque_classical_types() {
  static const OpTypeSet optypes{
      OpType::RNGSeed, OpType::RNGBound, OpType::RNGIndex, OpType::RNGNum,
      OpType::JobShotNum};
  static std::unique_ptr<const OpTypeSet> gates =
      std::make_unique<const OpTypeSet>(optypes);
  return *gates;
}

bool is_metaop_type(OpType optype) {
  static const OpTypeSet metaops = {
      OpType::Input,     OpType::Output,     OpType::ClInput, OpType::ClOutput,
      OpType::WASMInput, OpType::WASMOutput, OpType::Create,  OpType::Discard,
      OpType::RNGInput,  OpType::RNGOutput};
  return find_in_set(optype, metaops);
}

bool is_barrier_type(OpType optype) { return optype == OpType::Barrier; }

bool is_initial_q_type(OpType optype) {
  return optype == OpType::Input || optype == OpType::Create;
}

bool is_final_q_type(OpType optype) {
  return optype == OpType::Output || optype == OpType::Discard;
}

bool is_initial_type(OpType optype) {
  return optype == OpType::Input || optype == OpType::Create ||
         optype == OpType::ClInput || optype == OpType::WASMInput ||
         optype == OpType::RNGInput;
}

bool is_final_type(OpType optype) {
  return optype == OpType::Output || optype == OpType::Discard ||
         optype == OpType::ClOutput || optype == OpType::WASMOutput ||
         optype == OpType::RNGOutput;
}

bool is_boundary_type(OpType optype) {
  return is_boundary_q_type(optype) || is_boundary_c_type(optype) ||
         is_boundary_w_type(optype) || is_boundary_r_type(optype);
}

bool is_boundary_q_type(OpType optype) {
  return is_initial_q_type(optype) || is_final_q_type(optype);
}

bool is_boundary_c_type(OpType optype) {
  return optype == OpType::ClInput || optype == OpType::ClOutput;
}

bool is_boundary_w_type(OpType optype) {
  return optype == OpType::WASMInput || optype == OpType::WASMOutput;
}

bool is_boundary_r_type(OpType optype) {
  return optype == OpType::RNGInput || optype == OpType::RNGOutput;
}

bool is_gate_type(OpType optype) {
  return find_in_set(optype, all_gate_types());
}

bool is_box_type(OpType optype) {
  static const OpTypeSet boxes = {
      OpType::CircBox,
      OpType::Unitary1qBox,
      OpType::Unitary2qBox,
      OpType::Unitary3qBox,
      OpType::ExpBox,
      OpType::PauliExpBox,
      OpType::PauliExpPairBox,
      OpType::PauliExpCommutingSetBox,
      OpType::TermSequenceBox,
      OpType::CustomGate,
      OpType::CliffBox,
      OpType::PhasePolyBox,
      OpType::QControlBox,
      OpType::MultiplexorBox,
      OpType::MultiplexedRotationBox,
      OpType::MultiplexedU2Box,
      OpType::MultiplexedTensoredU2Box,
      OpType::StatePreparationBox,
      OpType::DiagonalBox,
      OpType::ConjugationBox,
      OpType::ProjectorAssertionBox,
      OpType::StabiliserAssertionBox,
      OpType::UnitaryTableauBox,
      OpType::ToffoliBox,
      OpType::DummyBox};
  return find_in_set(optype, boxes);
}

bool is_flowop_type(OpType optype) {
  static const OpTypeSet flowops = {
      OpType::Label, OpType::Branch, OpType::Goto, OpType::Stop};
  return find_in_set(optype, flowops);
}

bool is_rotation_type(OpType optype) {
  static const OpTypeSet rotation_gates = {
      OpType::Rx,      OpType::Ry,      OpType::Rz,    OpType::U1,
      OpType::CnRy,    OpType::CnRx,    OpType::CnRz,  OpType::CRz,
      OpType::CRx,     OpType::CRy,     OpType::CU1,   OpType::XXPhase,
      OpType::YYPhase, OpType::ZZPhase, OpType::ESWAP, OpType::ISWAP,
      OpType::XXPhase3};
  return find_in_set(optype, rotation_gates);
}

bool is_parameterised_pauli_rotation_type(OpType optype) {
  static const OpTypeSet parameterised_pauli_rotations = {
      OpType::Rx, OpType::Ry, OpType::Rz, OpType::U1};
  return find_in_set(optype, parameterised_pauli_rotations);
}

bool is_multi_qubit_type(OpType optype) {
  return find_in_set(optype, all_multi_qubit_types());
}

bool is_single_qubit_type(OpType optype) {
  return find_in_set(optype, all_single_qubit_types());
}

// the set of OpTypes that implement Gate_ptr->get_tk1_angles()
bool is_single_qubit_unitary_type(OpType optype) {
  return find_in_set(optype, all_single_qubit_unitary_types());
}

bool is_oneway_type(OpType optype) {
  // This set should contain only gates for which an dagger is nonsensical
  // or we do not yet have the dagger gate as an OpType.
  // If the gate can have an dagger, define it in the dagger() method.
  static const OpTypeSet no_defined_inverse = {
      OpType::Input,        OpType::Output,  OpType::Measure, OpType::ClInput,
      OpType::ClOutput,     OpType::Barrier, OpType::Reset,   OpType::Collapse,
      OpType::PhasePolyBox, OpType::Create,  OpType::Discard};
  return find_in_set(optype, no_defined_inverse);
}

bool is_clifford_type(OpType optype) {
  static const OpTypeSet clifford_gates = {
      OpType::Z,     OpType::X,    OpType::Y,        OpType::S,
      OpType::Sdg,   OpType::V,    OpType::Vdg,      OpType::SX,
      OpType::SXdg,  OpType::H,    OpType::CX,       OpType::CY,
      OpType::CZ,    OpType::SWAP, OpType::BRIDGE,   OpType::noop,
      OpType::ZZMax, OpType::ECR,  OpType::ISWAPMax, OpType::UnitaryTableauBox,
      OpType::Phase};
  return find_in_set(optype, clifford_gates);
}

bool is_projective_type(OpType optype) {
  return find_in_set(optype, all_projective_types());
}

bool is_classical_type(OpType optype) {
  return find_in_set(optype, all_classical_types());
}

bool is_controlled_gate_type(OpType optype) {
  return find_in_set(optype, all_controlled_gate_types());
}

bool is_opaque_classical_type(OpType optype) {
  return find_in_set(optype, all_opaque_classical_types());
}

}  // namespace tket

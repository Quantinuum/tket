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

#include "GatesData.hpp"

#include <catch2/catch_test_macros.hpp>

namespace tket {
namespace internal {

static GatesData get_data() {
  GatesData data;
  data.input_data[1][0] = {
      OpType::X,    OpType::Y,   OpType::Z,    OpType::S,   OpType::Sdg,
      OpType::T,    OpType::Tdg, OpType::V,    OpType::Vdg, OpType::H,
      OpType::noop, OpType::SX,  OpType::SXdg,
  };
  data.input_data[1][1] = {
      OpType::Rx,          OpType::Ry, OpType::Rz, OpType::U1,
      OpType::PhaseGadget,  // variable number of qubits
  };
  data.input_data[1][2] = {
      OpType::U2,
      OpType::PhasedX,
  };
  data.input_data[1][3] = {
      OpType::U3,
      OpType::TK1,
  };
  data.input_data[2][0] = {
      OpType::CX,       OpType::CY,       OpType::CZ,   OpType::CH,
      OpType::CV,       OpType::CVdg,     OpType::CSX,  OpType::CSXdg,
      OpType::CS,       OpType::CSdg,     OpType::SWAP, OpType::ZZMax,
      OpType::Sycamore, OpType::ISWAPMax, OpType::ECR,
  };
  data.input_data[2][1] = {
      OpType::CRx,         OpType::CRy,     OpType::CRz,
      OpType::CU1,         OpType::ISWAP,   OpType::XXPhase,
      OpType::YYPhase,     OpType::ZZPhase, OpType::ESWAP,
      OpType::PhaseGadget,  // variable number of qubits
  };
  data.input_data[2][2] = {
      OpType::PhasedISWAP,
      OpType::FSim,
      OpType::TwinPhasedX,
      OpType::PhasedXX,
  };
  data.input_data[2][3] = {
      OpType::CU3,
      OpType::TK2,
  };
  data.input_data[3][0] = {
      OpType::CCX,
      OpType::CnX,  // variable number of qubits
      OpType::CnY,  // variable number of qubits
      OpType::CnZ,  // variable number of qubits
      OpType::CSWAP, OpType::BRIDGE,
  };
  data.input_data[3][1] = {
      OpType::CnRy,         // variable number of qubits
      OpType::PhaseGadget,  // variable number of qubits
      OpType::XXPhase3,
  };
  data.input_data[3][2] = {
      OpType::NPhasedX,  // variable number of qubits
  };
  data.input_data[4][0] = {
      OpType::CnX,  // variable number of qubits
      OpType::CnY,  // variable number of qubits
      OpType::CnZ,  // variable number of qubits
  };
  data.input_data[4][1] = {
      OpType::CnRy,         // variable number of qubits
      OpType::PhaseGadget,  // variable number of qubits
  };
  data.min_number_of_qubits_for_variable_qubit_type[OpType::CnX] = 1;
  data.min_number_of_qubits_for_variable_qubit_type[OpType::CnY] = 1;
  data.min_number_of_qubits_for_variable_qubit_type[OpType::CnZ] = 1;
  data.min_number_of_qubits_for_variable_qubit_type[OpType::CnRy] = 1;
  data.min_number_of_qubits_for_variable_qubit_type[OpType::NPhasedX] = 0;
  data.min_number_of_qubits_for_variable_qubit_type[OpType::PhaseGadget] = 0;
  return data;
}

const GatesData& GatesData::get() {
  static const auto gates_data = get_data();
  return gates_data;
}

}  // namespace internal
}  // namespace tket

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

#include "tket/Gate/GateUnitaryMatrix.hpp"

#include <cmath>
#include <sstream>
#include <tkassert/Assert.hpp>

#include "GateUnitaryMatrixVariableQubits.hpp"
#include "GateUnitarySparseMatrix.hpp"
#include "tket/Gate/Gate.hpp"
#include "tket/Gate/GateUnitaryMatrixError.hpp"
#include "tket/Gate/GateUnitaryMatrixImplementations.hpp"
#include "tket/Gate/GateUnitaryMatrixUtils.hpp"
#include "tket/OpType/OpTypeInfo.hpp"

// This is just for the main Gate -> matrix function, so the only part
// which actually uses the rest of tket. This is nothing but a wrapper around
// the basic named functions like x(), etc. (with extra error checks).

// NOTE: MatrixXcd objects are returned by a series of functions;
// if the OpType is unrecognised, it returns a 0x0 matrix.
// Eventually, when it is recognised, a nonempty matrix will be returned,
// or an error thrown if nothing recognises it.

namespace tket {

using internal::GateUnitaryMatrixImplementations;
using internal::GateUnitaryMatrixUtils;

#ifdef CASE_RETURN_0P
#error "Macro already defined!"
#endif
#define CASE_RETURN_0P(function_name)                                        \
  case OpType::function_name:                                                \
    GateUnitaryMatrixUtils::check_and_throw_upon_wrong_number_of_parameters( \
        OpType::function_name, number_of_qubits, parameters, 0);             \
    return GateUnitaryMatrixImplementations::function_name();

#ifdef CASE_RETURN_1P
#error "Macro already defined!"
#endif
#define CASE_RETURN_1P(function_name)                                        \
  case OpType::function_name:                                                \
    GateUnitaryMatrixUtils::check_and_throw_upon_wrong_number_of_parameters( \
        OpType::function_name, number_of_qubits, parameters, 1);             \
    return GateUnitaryMatrixImplementations::function_name(parameters[0]);

#ifdef CASE_RETURN_2P
#error "Macro already defined!"
#endif
#define CASE_RETURN_2P(function_name)                                        \
  case OpType::function_name:                                                \
    GateUnitaryMatrixUtils::check_and_throw_upon_wrong_number_of_parameters( \
        OpType::function_name, number_of_qubits, parameters, 2);             \
    return GateUnitaryMatrixImplementations::function_name(                  \
        parameters[0], parameters[1]);

#ifdef CASE_RETURN_3P
#error "Macro already defined!"
#endif
#define CASE_RETURN_3P(function_name)                                        \
  case OpType::function_name:                                                \
    GateUnitaryMatrixUtils::check_and_throw_upon_wrong_number_of_parameters( \
        OpType::function_name, number_of_qubits, parameters, 3);             \
    return GateUnitaryMatrixImplementations::function_name(                  \
        parameters[0], parameters[1], parameters[2]);

// Only for op types with a fixed, known number of qubits;
// throws if the number of parameters is wrong.
// However, does NOT check the number of qubits.
static Eigen::MatrixXcd get_unitary_or_throw(
    OpType op_type, unsigned number_of_qubits,
    const std::vector<double>& parameters) {
  switch (op_type) {
    CASE_RETURN_0P(X)
    CASE_RETURN_0P(Y)
    CASE_RETURN_0P(Z)
    CASE_RETURN_0P(S)
    CASE_RETURN_0P(Sdg)
    CASE_RETURN_0P(T)
    CASE_RETURN_0P(Tdg)
    CASE_RETURN_0P(V)
    CASE_RETURN_0P(Vdg)
    CASE_RETURN_0P(H)
    CASE_RETURN_0P(BRIDGE)
    CASE_RETURN_0P(noop)
    CASE_RETURN_0P(ECR)
    CASE_RETURN_0P(SX)
    CASE_RETURN_0P(SXdg)
    CASE_RETURN_0P(CSWAP)
    CASE_RETURN_0P(CCX)
    CASE_RETURN_0P(CX)
    CASE_RETURN_0P(CY)
    CASE_RETURN_0P(CZ)
    CASE_RETURN_0P(CH)
    CASE_RETURN_0P(CV)
    CASE_RETURN_0P(CVdg)
    CASE_RETURN_0P(CSX)
    CASE_RETURN_0P(CSXdg)
    CASE_RETURN_0P(CS)
    CASE_RETURN_0P(CSdg)
    CASE_RETURN_0P(SWAP)
    CASE_RETURN_0P(ZZMax)
    CASE_RETURN_0P(Sycamore)
    CASE_RETURN_0P(ISWAPMax)
#undef CASE_RETURN_0P
    CASE_RETURN_1P(Phase)
    CASE_RETURN_1P(Rx)
    CASE_RETURN_1P(Ry)
    CASE_RETURN_1P(Rz)
    CASE_RETURN_1P(U1)
    CASE_RETURN_1P(GPI)
    CASE_RETURN_1P(GPI2)
    CASE_RETURN_1P(CRx)
    CASE_RETURN_1P(CRy)
    CASE_RETURN_1P(CRz)
    CASE_RETURN_1P(CU1)
    CASE_RETURN_1P(ISWAP)
    CASE_RETURN_1P(XXPhase)
    CASE_RETURN_1P(YYPhase)
    CASE_RETURN_1P(ZZPhase)
    CASE_RETURN_1P(XXPhase3)
    CASE_RETURN_1P(ESWAP)
#undef CASE_RETURN_1P
    CASE_RETURN_2P(U2)
    CASE_RETURN_2P(PhasedX)
    CASE_RETURN_2P(TwinPhasedX)
    CASE_RETURN_2P(PhasedXX)
    CASE_RETURN_2P(PhasedISWAP)
    CASE_RETURN_2P(FSim)
#undef CASE_RETURN_2P
    CASE_RETURN_3P(CU3)
    CASE_RETURN_3P(U3)
    CASE_RETURN_3P(AAMS)
    CASE_RETURN_3P(TK1)
    CASE_RETURN_3P(TK2)
#undef CASE_RETURN_3P
    default: {
      std::stringstream ss;
      ss << GateUnitaryMatrixUtils::get_error_prefix(
                op_type, number_of_qubits, parameters)
         << "unrecognised Op type";
      throw BadOpType(ss.str(), op_type);
    }
  }
}

// It's already been checked not to be one of the special cases
// having a variable number of qubits.
static Eigen::MatrixXcd get_unitary_for_ordinary_fixed_size_case(
    OpType op_type, unsigned number_of_qubits,
    const std::vector<double>& parameters) {
  const Eigen::MatrixXcd matr =
      get_unitary_or_throw(op_type, number_of_qubits, parameters);

  TKET_ASSERT(matr.cols() == matr.rows());
  const auto expected_number_of_qubits = get_number_of_qubits(matr.cols());
  if (expected_number_of_qubits == number_of_qubits) {
    return matr;
  }
  std::stringstream ss;
  ss << GateUnitaryMatrixUtils::get_error_prefix(
            op_type, number_of_qubits, parameters)
     << "wrong number of qubits (expected " << expected_number_of_qubits << ")";
  throw GateUnitaryMatrixError(
      ss.str(), GateUnitaryMatrixError::Cause::INPUT_ERROR);
}

Eigen::MatrixXcd GateUnitaryMatrix::get_unitary(
    OpType op_type, unsigned number_of_qubits,
    const std::vector<double>& parameters) {
  const internal::GateUnitaryMatrixVariableQubits variable_qubits_data(op_type);
  if (!variable_qubits_data.is_known_type()) {
    return get_unitary_for_ordinary_fixed_size_case(
        op_type, number_of_qubits, parameters);
  }
  const auto expected_number_of_parameters =
      variable_qubits_data.get_number_of_parameters();
  if (parameters.size() == expected_number_of_parameters) {
    return variable_qubits_data.get_dense_unitary(number_of_qubits, parameters);
  }
  std::stringstream ss;
  ss << GateUnitaryMatrixUtils::get_error_prefix(
            op_type, number_of_qubits, parameters)
     << "wrong number of parameters (expected " << expected_number_of_parameters
     << ")";
  throw GateUnitaryMatrixError(
      ss.str(), GateUnitaryMatrixError::Cause::INPUT_ERROR);
}

Eigen::MatrixXcd GateUnitaryMatrix::get_unitary(const Gate& gate) {
  const auto parameters = GateUnitaryMatrixUtils::get_checked_parameters(gate);
  return get_unitary(gate.get_type(), gate.n_qubits(), parameters);
}

std::vector<TripletCd> GateUnitaryMatrix::get_unitary_triplets(
    const Gate& gate, double abs_epsilon) {
  auto triplets = internal::GateUnitarySparseMatrix::get_unitary_triplets(
      gate, abs_epsilon);
  if (triplets.empty()) {
    // Not recognised as a specific sparse type, so just get the dense matrix
    const auto unitary_matr = get_unitary(gate);
    triplets = get_triplets(unitary_matr, abs_epsilon);
  }
  return triplets;
}

}  // namespace tket
